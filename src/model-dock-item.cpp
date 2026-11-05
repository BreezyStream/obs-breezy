#include "model-dock-item.hpp"
#include "model.hpp"
#include "toggle-switch.hpp"

#include <QLabel>
#include <QComboBox>
#include <QProgressBar>
#include <QHBoxLayout>
#include <QVBoxLayout>
#include <QPainter>
#include <QMouseEvent>
#include <QContextMenuEvent>

#include <algorithm>

static constexpr double kInferenceRates[] = { 1.0, 5.0, 10.0, 15.0, 30.0, 60.0 };

static constexpr int kScoreSteps = 1000;

static constexpr qreal kCardRadius = 6.0;
static constexpr qreal kCardTint = 0.06;
static constexpr qreal kCardTintHover = 0.10;

static constexpr qreal kChevronSize = 3.5;
static constexpr qreal kChevronOpacity = 0.6;

static QColor
mixColor(const QColor& a, const QColor& b, qreal amount) {
	return QColor::fromRgbF(
		a.redF()   + (b.redF()   - a.redF())   * amount,
		a.greenF() + (b.greenF() - a.greenF()) * amount,
		a.blueF()  + (b.blueF()  - a.blueF())  * amount
	);
}

static QString
formatRate(double rate) {
	return QString::number(rate, 'g', 3);
}

static QString
formatScore(float score) {
	return QString("%1%").arg(qRound(score * 100.0f));
}

//-----------------------------------------------------------------------------
// Constructor
//-----------------------------------------------------------------------------

ModelDockItem::ModelDockItem(Model* model, QWidget* parent) : QFrame(parent), m_model(model) {
	setup();
	refresh();
}

//-----------------------------------------------------------------------------
// Public
//-----------------------------------------------------------------------------

void
ModelDockItem::setExpanded(bool expanded) {

	m_expanded = expanded;

	m_details->setVisible(expanded);

	update();

	if (expanded == false) {
		clearScores();
	}
}

void
ModelDockItem::setStatus(const QString& text, const QColor& color, const QString& tip, bool editable) {

	m_editable = editable;

	m_status->setText(text);
	m_status->setToolTip(tip);
	m_status->setStyleSheet(QString(
		"color: %1;"
		"background: rgba(%2, %3, %4, 40);"
		"border-radius: 9px;"
		"padding: 0px 6px;"
		"margin: 0px;"
	).arg(color.name()).arg(color.red()).arg(color.green()).arg(color.blue()));

	refresh();
}

void
ModelDockItem::setScores(const QHash<QString, float>& scores) {

	/*
	 * Scores arrive at the inference rate, they're only drawn while the item
	 * is expanded.
	 */

	if (m_expanded == false) {
		return;
	}

	for (auto it = scores.begin(); it != scores.end(); ++it) {

		auto entry = m_scores.find(it.key());
		if (entry == m_scores.end()) {
			continue;
		}

		const float score = std::clamp(it.value(), 0.0f, 1.0f);

		entry->value->setText(formatScore(score));
		entry->bar->setValue(qRound(score * kScoreSteps));
	}
}

void
ModelDockItem::clearScores() {
	for (auto& entry : m_scores) {
		entry.value->setText("–");
		entry.bar->setValue(0);
	}
}

void
ModelDockItem::refresh() {

	m_updating = true;

	m_enabled->setEnabled(m_editable);
	m_enabled->setChecked(m_model->enabled());
	m_enabled->setToolTip(m_editable ? "Turn off to stop running this model." : "");

	setupRates();

	m_rate->setEnabled(m_editable);
	m_rate->setToolTip("Predictions per second.");

	m_updating = false;
}

//-----------------------------------------------------------------------------
// Protected
//-----------------------------------------------------------------------------

void
ModelDockItem::paintEvent(QPaintEvent* event) {

	/*
	 * The card is the dock background tinted toward the text color, so it
	 * follows the OBS theme whether it's dark or light.
	 */

	Q_UNUSED(event);

	const auto bg = palette().color(QPalette::Base);
	const auto fg = palette().color(QPalette::Text);

	QPainter painter(this);
	painter.setRenderHint(QPainter::Antialiasing);
	painter.setPen(Qt::NoPen);
	painter.setBrush(mixColor(bg, fg, underMouse() ? kCardTintHover : kCardTint));
	painter.drawRoundedRect(QRectF(rect()), kCardRadius, kCardRadius);

	/*
	 * The chevron points right when collapsed and down when expanded, it's
	 * drawn here so it has no button shape around it.
	 */

	auto color = fg;
	color.setAlphaF(kChevronOpacity);

	QPen pen(color, 1.5);
	pen.setCapStyle(Qt::RoundCap);
	pen.setJoinStyle(Qt::RoundJoin);

	const auto center = QRectF(m_arrow->geometry()).translated(m_header->pos()).center();
	const qreal s = kChevronSize;

	QPolygonF chevron;

	if (m_expanded) {
		chevron << QPointF(-s, -s / 2) << QPointF(0, s / 2) << QPointF(s, -s / 2);
	} else {
		chevron << QPointF(-s / 2, -s) << QPointF(s / 2, 0) << QPointF(-s / 2, s);
	}

	painter.setPen(pen);
	painter.setBrush(Qt::NoBrush);
	painter.drawPolyline(chevron.translated(center));
}

void
ModelDockItem::mousePressEvent(QMouseEvent* event) {

	/*
	 * Clicking anywhere on the header toggles the item, the switch handles
	 * its own clicks.
	 */

	if (event->button() == Qt::LeftButton &&
		m_header->geometry().contains(event->position().toPoint())) {
		setExpanded(m_expanded == false);
		return;
	}

	QFrame::mousePressEvent(event);
}

void
ModelDockItem::contextMenuEvent(QContextMenuEvent* event) {
	emit menuRequested(event->globalPos());
}

//-----------------------------------------------------------------------------
// Private
//-----------------------------------------------------------------------------

void
ModelDockItem::setup() {

	setAttribute(Qt::WA_Hover, true);

	auto layout = new QVBoxLayout(this);
	layout->setContentsMargins(10, 8, 8, 8);
	layout->setSpacing(8);

	//
	// Header
	//

	m_header = new QWidget(this);

	auto headerLayout = new QHBoxLayout(m_header);
	headerLayout->setContentsMargins(0, 0, 0, 0);
	headerLayout->setSpacing(8);


	auto text = new QWidget(m_header);
	text->setSizePolicy(QSizePolicy::Ignored, QSizePolicy::Preferred);

	auto textLayout = new QVBoxLayout(text);
	textLayout->setContentsMargins(0, 0, 0, 0);
	textLayout->setSpacing(1);

	m_title = new QLabel(m_model->title(), text);
	m_title->setSizePolicy(QSizePolicy::Ignored, QSizePolicy::Preferred);
	m_title->setToolTip(m_model->title());

	m_version = new QLabel(QString("Version %1").arg(m_model->version()), text);
	m_version->setSizePolicy(QSizePolicy::Ignored, QSizePolicy::Preferred);
	m_version->setEnabled(false);

	textLayout->addWidget(m_title);
	textLayout->addWidget(m_version);

	m_status = new QLabel(m_header);

	auto statusFont = m_status->font();
	statusFont.setPointSizeF(statusFont.pointSizeF() * 0.85);
	m_status->setFont(statusFont);
	m_status->setFixedHeight(18);
	m_status->setAlignment(Qt::AlignCenter);

	m_enabled = new ToggleSwitch(m_header);

	m_arrow = new QWidget(m_header);
	m_arrow->setFixedSize(12, 16);

	headerLayout->addWidget(text, 1);
	headerLayout->addWidget(m_status);
	headerLayout->addWidget(m_enabled);
	headerLayout->addWidget(m_arrow);

	layout->addWidget(m_header);

	//
	// Details
	//

	m_details = new QWidget(this);

	auto detailsLayout = new QVBoxLayout(m_details);
	detailsLayout->setContentsMargins(0, 0, 0, 2);
	detailsLayout->setSpacing(8);

	auto settings = new QWidget(m_details);

	auto settingsLayout = new QHBoxLayout(settings);
	settingsLayout->setContentsMargins(0, 0, 0, 0);
	settingsLayout->setSpacing(6);

	auto rateLabel = new QLabel("Inference rate", settings);
	rateLabel->setEnabled(false);

	m_rate = new QComboBox(settings);

	settingsLayout->addWidget(rateLabel, 1);
	settingsLayout->addWidget(m_rate);

	detailsLayout->addWidget(settings);

	setupScores(m_details);

	layout->addWidget(m_details);

	m_details->setVisible(false);

	connect(m_enabled, &ToggleSwitch::toggled, this, [this] (bool checked) {
		if (m_updating == false) emit enabledChanged(checked);
	});

	connect(m_rate, &QComboBox::activated, this, &ModelDockItem::onRateEdited);
}

void
ModelDockItem::setupRates() {

	/*
	 * The presets, plus the model's default and current rates when they're
	 * not one of them. The default is labelled, picking it clears the
	 * override.
	 */

	const double defaultRate = m_model->defaultInferenceRate();
	const double currentRate = m_model->inferenceRate();

	QVector<double> rates(std::begin(kInferenceRates), std::end(kInferenceRates));

	for (double rate : { defaultRate, currentRate }) {

		const bool found = std::any_of(rates.begin(), rates.end(), [rate] (double value) {
			return qFuzzyCompare(value, rate);
		});

		if (found == false) {
			rates.append(rate);
		}
	}

	std::sort(rates.begin(), rates.end());

	m_rate->clear();

	for (double rate : rates) {

		auto text = QString("%1 FPS").arg(formatRate(rate));

		if (qFuzzyCompare(rate, defaultRate)) {
			text += " (Default)";
		}

		m_rate->addItem(text, rate);

		if (qFuzzyCompare(rate, currentRate)) {
			m_rate->setCurrentIndex(m_rate->count() - 1);
		}
	}
}

void
ModelDockItem::setupScores(QWidget* parent) {

	const auto& ids = m_model->labels();
	const auto& titles = m_model->labelTitles();

	for (int i = 0; i < ids.size(); i++) {

		auto row = new QWidget(parent);

		auto rowLayout = new QVBoxLayout(row);
		rowLayout->setContentsMargins(0, 0, 0, 0);
		rowLayout->setSpacing(4);

		auto line = new QWidget(row);

		auto lineLayout = new QHBoxLayout(line);
		lineLayout->setContentsMargins(0, 0, 0, 0);
		lineLayout->setSpacing(6);

		auto title = i < titles.size() && titles[i].isEmpty() == false ? titles[i] : ids[i];

		auto name = new QLabel(title, line);
		name->setSizePolicy(QSizePolicy::Ignored, QSizePolicy::Preferred);
		name->setToolTip(title);

		auto value = new QLabel("–", line);
		value->setAlignment(Qt::AlignRight | Qt::AlignVCenter);
		value->setEnabled(false);

		lineLayout->addWidget(name, 1);
		lineLayout->addWidget(value);

		auto bar = new QProgressBar(row);
		bar->setRange(0, kScoreSteps);
		bar->setValue(0);
		bar->setTextVisible(false);
		bar->setFixedHeight(6);

		rowLayout->addWidget(line);
		rowLayout->addWidget(bar);

		parent->layout()->addWidget(row);

		m_scores.insert(ids[i], Score { value, bar });
	}
}

//-----------------------------------------------------------------------------
// Private Slots
//-----------------------------------------------------------------------------

void
ModelDockItem::onRateEdited() {

	if (m_updating) {
		return;
	}

	const double rate = m_rate->currentData().toDouble();

	if (qFuzzyCompare(rate, m_model->inferenceRate())) {
		return;
	}

	// Setting the default value back clears the override.
	emit inferenceRateChanged(qFuzzyCompare(rate, m_model->defaultInferenceRate()) ? 0.0 : rate);
}
