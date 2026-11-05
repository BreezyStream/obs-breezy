#pragma once

#include <QAbstractButton>

/*
 * An on/off switch drawn with the theme's palette, the track uses the accent
 * color when checked.
 */

class ToggleSwitch : public QAbstractButton {

	Q_OBJECT

public:

	explicit ToggleSwitch(QWidget* parent = nullptr);

	QSize sizeHint() const override;

protected:

	void paintEvent(QPaintEvent* event) override;

};
