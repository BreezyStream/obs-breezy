#include "model-dock.hpp"
#include "model-dock-item.hpp"
#include "model.hpp"
#include "object-classification-model-prediction.hpp"

#include "ui_model-dock.h"

#include <obs.h>

#include <QMenu>
#include <QLabel>
#include <QCheckBox>
#include <QHBoxLayout>
#include <QVBoxLayout>
#include <QToolBar>
#include <QScrollArea>

static QString
formatRate(double rate) {
	return QString::number(rate, 'f', 1);
}

//-----------------------------------------------------------------------------
// Constructor
//-----------------------------------------------------------------------------

ModelDock::ModelDock(QWidget* parent) : QWidget(parent) {
	setWindowTitle("Breezy");

	// Lets the theme's dock rule paint the background and border, like the native docks.
	setAttribute(Qt::WA_StyledBackground, true);
	ui = new Ui::ModelDock();
	ui->setupUi(this);
	setup();
}

//-----------------------------------------------------------------------------
// Destructor
//-----------------------------------------------------------------------------

ModelDock::~ModelDock() {
	delete ui;
}

//-----------------------------------------------------------------------------
// Public
//-----------------------------------------------------------------------------

void
ModelDock::setUnifyVersions(bool unify) {
	QSignalBlocker blocker(m_unifyVersions);
	m_unifyVersions->setChecked(unify);
}

void
ModelDock::updateModelItem(Model* model, ModelStatus status) {
	updateModelItem(model, status, 0.0);
}

void
ModelDock::updateModelItem(Model* model, ModelStatus status, float progress) {

	const auto key = model->key();

	if (m_entries.contains(key) == false) {

		auto item = new ModelDockItem(model, ui->listContent);

		connect(item, &ModelDockItem::enabledChanged, this, [this, key] (bool enabled) {
			emit modelEnabledChanged(key, enabled);
		});

		connect(item, &ModelDockItem::inferenceRateChanged, this, [this, key] (double rate) {
			emit modelInferenceRateChanged(key, rate);
		});

		connect(item, &ModelDockItem::menuRequested, this, [this, model] (const QPoint& pos) {
			showMenu(model, pos);
		});

		/*
		 * Items go before the stretch that keeps them at the top, they start
		 * collapsed.
		 */

		auto layout = static_cast<QVBoxLayout*>(ui->listContent->layout());
		auto index = layout->count() - 1;

		layout->insertWidget(index, item);

		item->setExpanded(false);

		m_entries.insert(key, Entry { item });
		m_empty->setVisible(false);
	}

	auto& entry = m_entries[key];
	entry.status = status;
	entry.progress = progress;

	render(key);
}

void
ModelDock::updateModelScores(const QVector<PredictionPtr>& preds) {

	QHash<Model*, QHash<QString, float>> scores;

	for (const auto& pred : preds) {
		if (auto value = qSharedPointerDynamicCast<ObjectClassificationModelPrediction>(pred)) {
			scores[value->model()].insert(value->label(), value->score());
		}
	}

	for (auto it = scores.begin(); it != scores.end(); ++it) {

		auto entry = m_entries.find(it.key()->key());
		if (entry == m_entries.end()) {
			continue;
		}

		entry->item->setScores(it.value());
	}
}

void
ModelDock::refreshModelItem(Model* model) {

	if (m_entries.contains(model->key()) == false) {
		return;
	}

	render(model->key());
}

//-----------------------------------------------------------------------------
// Private
//-----------------------------------------------------------------------------

void
ModelDock::setup() {

	auto layout = static_cast<QVBoxLayout*>(ui->listContent->layout());
	layout->setContentsMargins(6, 6, 6, 6);
	layout->setSpacing(4);

	// Same background as the lists in the native docks.
	ui->list->setBackgroundRole(QPalette::Base);
	ui->list->viewport()->setBackgroundRole(QPalette::Base);
	ui->listContent->setBackgroundRole(QPalette::Base);
	ui->listContent->setAutoFillBackground(true);

	m_empty = new QLabel("No models loaded. Models load when an overlay that uses them opens in a Browser source.", ui->listContent);
	m_empty->setWordWrap(true);
	m_empty->setAlignment(Qt::AlignCenter);
	m_empty->setContentsMargins(16, 16, 16, 16);
	m_empty->setEnabled(false);

	layout->addWidget(m_empty);
	layout->addStretch(1);

	m_unifyVersions = new QCheckBox("Use the most recent version of each model");
	m_unifyVersions->setToolTip("When widgets load different versions of the same model, run only the most recent one and send its predictions to all of them. Saves CPU.");
	m_unifyVersions->setChecked(true);

	// The theme draws the top border on dock toolbars, like the native Scenes and Sources docks.
	auto footer = new QWidget();
	auto footerLayout = new QHBoxLayout(footer);
	footerLayout->setContentsMargins(8, 8, 8, 8);
	footerLayout->addWidget(m_unifyVersions);

	/*
	 * The padding brings the bar to the 32px the theme uses for the native
	 * Scenes and Sources bars, with the checkbox centered.
	 */

	footer->setSizePolicy(QSizePolicy::Preferred, QSizePolicy::Expanding);
	footerLayout->setAlignment(m_unifyVersions, Qt::AlignVCenter);

	ui->toolbar->addWidget(footer);

	connect(m_unifyVersions, &QCheckBox::toggled, this, &ModelDock::unifyVersionsChanged);
}

void
ModelDock::render(const QString& key) {

	const auto& entry = m_entries[key];

	auto item = entry.item;
	auto model = item->model();

	QString status = "";
	QString statusTip = "";
	QColor  color;

	switch (entry.status) {

	case ModelStatus::DOWNLOAD_BEGAN:
		status = "Downloading";
		color = QColor(0, 111, 255);
		break;

	case ModelStatus::DOWNLOAD_ENDED:
		status = "Downloaded";
		color = QColor(0, 111, 255);
		break;

	case ModelStatus::DOWNLOADING:
		status = QString("Downloading %1%").arg(qRound(entry.progress * 100));
		color = QColor(0, 111, 255);
		break;

	case ModelStatus::ACTIVE:

		if (model->enabled() == false) {

			status = "Disabled";
			color = QColor(140, 140, 140);

		} else if (auto newer = model->supersededBy()) {

			status = QString("Using %1").arg(newer->version());
			statusTip = QString("Version %1 of this model is also loaded and runs in its place.").arg(newer->version());
			color = QColor(140, 140, 140);

		} else {

			status = "Active";
			color = QColor(38, 191, 107);

		}

		break;

	case ModelStatus::FAILED:
		status = "Failed";
		color = QColor(240, 93, 93);
		break;
	}

	const bool editable = entry.status == ModelStatus::ACTIVE;

	/*
	 * A model that stops running keeps no stale scores.
	 */

	if (editable == false ||
		model->enabled() == false ||
		model->supersededBy()) {
		item->clearScores();
	}

	item->setStatus(status, color, statusTip, editable);
}

void
ModelDock::showMenu(Model* model, const QPoint& pos) {

	const auto key = model->key();

	if (m_entries.contains(key) == false ||
		m_entries[key].status != ModelStatus::ACTIVE) {
		return;
	}

	QMenu menu(this);

	auto toggle = menu.addAction(model->enabled() ? "Disable" : "Enable");
	auto reset = menu.addAction(QString("Reset Inference Rate (%1 FPS)").arg(formatRate(model->defaultInferenceRate())));
	reset->setEnabled(model->inferenceRateOverridden());

	auto chosen = menu.exec(pos);

	if (chosen == toggle) {
		emit modelEnabledChanged(key, model->enabled() == false);
	} else if (chosen == reset) {
		emit modelInferenceRateChanged(key, 0.0);
	}
}
