#include "toggle-switch.hpp"

#include <QPainter>

static constexpr int kTrackWidth = 28;
static constexpr int kTrackHeight = 16;
static constexpr int kKnobMargin = 2;

//-----------------------------------------------------------------------------
// Constructor
//-----------------------------------------------------------------------------

ToggleSwitch::ToggleSwitch(QWidget* parent) : QAbstractButton(parent) {
	setCheckable(true);
	setCursor(Qt::PointingHandCursor);
	setSizePolicy(QSizePolicy::Fixed, QSizePolicy::Fixed);
}

//-----------------------------------------------------------------------------
// Public
//-----------------------------------------------------------------------------

QSize
ToggleSwitch::sizeHint() const {
	return QSize(kTrackWidth, kTrackHeight);
}

//-----------------------------------------------------------------------------
// Protected
//-----------------------------------------------------------------------------

void
ToggleSwitch::paintEvent(QPaintEvent* event) {

	Q_UNUSED(event);

	const auto& pal = palette();

	auto track = isChecked() ? pal.color(QPalette::Highlight) : pal.color(QPalette::Mid);
	auto knob = isChecked() ? pal.color(QPalette::HighlightedText) : pal.color(QPalette::Light);

	if (isEnabled() == false) {
		track.setAlphaF(0.4);
		knob.setAlphaF(0.6);
	}

	const QRectF rect((width() - kTrackWidth) / 2.0, (height() - kTrackHeight) / 2.0, kTrackWidth, kTrackHeight);

	const qreal size = kTrackHeight - kKnobMargin * 2;
	const qreal x = isChecked() ? rect.right() - kKnobMargin - size : rect.left() + kKnobMargin;

	QPainter painter(this);
	painter.setRenderHint(QPainter::Antialiasing);
	painter.setPen(Qt::NoPen);

	painter.setBrush(track);
	painter.drawRoundedRect(rect, kTrackHeight / 2.0, kTrackHeight / 2.0);

	painter.setBrush(knob);
	painter.drawEllipse(QRectF(x, rect.top() + kKnobMargin, size, size));
}
