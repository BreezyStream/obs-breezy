#pragma once

#include <QFrame>
#include <QHash>
#include <QColor>

class Model;
class QLabel;
class QComboBox;
class ToggleSwitch;
class QProgressBar;

/*
 * One model in the Breezy dock. Collapsed, it shows the model, its version
 * and its status. Expanded, it shows its inference rate and the live score of each
 * of its labels.
 */

class ModelDockItem : public QFrame {

	Q_OBJECT

public:

	explicit ModelDockItem(Model* model, QWidget* parent = nullptr);

	Model* model() const {
		return m_model;
	}

	bool expanded() const {
		return m_expanded;
	}

	void setExpanded(bool expanded);
	void setStatus(const QString& text, const QColor& color, const QString& tip, bool editable);
	void setScores(const QHash<QString, float>& scores);
	void clearScores();
	void refresh();

signals:

	void enabledChanged(bool enabled);
	void inferenceRateChanged(double rate);
	void menuRequested(const QPoint& pos);

protected:

	void paintEvent(QPaintEvent* event) override;
	void mousePressEvent(QMouseEvent* event) override;
	void contextMenuEvent(QContextMenuEvent* event) override;

private:

	struct Score {
		QLabel* value = nullptr;
		QProgressBar* bar = nullptr;
	};

	Model* m_model = nullptr;

	QWidget* m_header = nullptr;
	QWidget* m_arrow = nullptr;
	ToggleSwitch* m_enabled = nullptr;
	QLabel* m_title = nullptr;
	QLabel* m_version = nullptr;
	QLabel* m_status = nullptr;

	QWidget* m_details = nullptr;
	QComboBox* m_rate = nullptr;

	QHash<QString, Score> m_scores;

	bool m_expanded = false;
	bool m_editable = false;
	bool m_updating = false;

	void setup();
	void setupScores(QWidget* parent);
	void setupRates();

private slots:

	void onRateEdited();

};
