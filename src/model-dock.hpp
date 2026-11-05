#pragma once

#include "prediction.hpp"

#include <QHash>
#include <QPoint>
#include <QVector>
#include <QWidget>

namespace Ui { class ModelDock; }

class Model;
class ModelDockItem;
class QLabel;
class QCheckBox;
class ModelDock : public QWidget {

	Q_OBJECT

public:

	enum ModelStatus {
		DOWNLOAD_BEGAN,
		DOWNLOAD_ENDED,
		DOWNLOADING,
		ACTIVE,
		FAILED
	};

	explicit ModelDock(QWidget* parent = nullptr);

	~ModelDock();

	void setUnifyVersions(bool unify);

	void updateModelItem(Model* model, ModelStatus status);
	void updateModelItem(Model* model, ModelStatus status, float progress);
	void updateModelScores(const QVector<PredictionPtr>& preds);
	void refreshModelItem(Model* model);

signals:

	void modelEnabledChanged(const QString& key, bool enabled);
	void modelInferenceRateChanged(const QString& key, double rate);
	void unifyVersionsChanged(bool unify);

private:

	struct Entry {
		ModelDockItem* item = nullptr;
		ModelStatus status = DOWNLOAD_BEGAN;
		float progress = 0.0f;
	};

	Ui::ModelDock* ui = nullptr;

	QHash<QString, Entry> m_entries;
	QLabel* m_empty = nullptr;
	QCheckBox* m_unifyVersions = nullptr;

	void setup();
	void render(const QString& key);
	void showMenu(Model* model, const QPoint& pos);

};
