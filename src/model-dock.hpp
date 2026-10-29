#pragma once

#include <QHash>
#include <QWidget>
#include <QStandardItemModel>

namespace Ui { class ModelDock; }

class Model;
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

	void updateModelTableItem(Model* model, ModelStatus status);
	void updateModelTableItem(Model* model, ModelStatus status, float progress);
	
private:

	Ui::ModelDock* ui = nullptr;

	QHash<QString, int>m_modelTableRows;
	QStandardItemModel* m_modelTableItemModel;
	QStandardItemModel* m_inferTableItemModel;

	void setup();

	QStandardItem* createItem(const QString& label);
	QStandardItem* createItem(double label);
	QStandardItem* createItem(float label);
};
