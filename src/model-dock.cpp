
#include "model-dock.hpp"
#include "model.hpp"

#include "ui_model-dock.h"

#include <obs.h>

#include <QTableView>
#include <QStandardItemModel>

//-----------------------------------------------------------------------------
// Constructor
//-----------------------------------------------------------------------------

ModelDock::ModelDock(QWidget* parent) : QWidget(parent) {
	setWindowTitle("Breezy");
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
ModelDock::updateModelTableItem(Model* model, ModelStatus status) {
	updateModelTableItem(model, status, 0.0);
}

void
ModelDock::updateModelTableItem(Model* model, ModelStatus status, float progress) {

	int row = m_modelTableRows.value(model->key(), -1);
	if (row < 0) {

		row = m_modelTableItemModel->rowCount();

		m_modelTableRows.insert(model->key(), row);

		m_modelTableItemModel->setItem(row, 0, createItem(model->title()));
		m_modelTableItemModel->setItem(row, 1, createItem(model->version()));
		m_modelTableItemModel->setItem(row, 2, createItem(""));
		m_modelTableItemModel->setItem(row, 3, createItem(""));
		m_modelTableItemModel->setItem(row, 4, createItem(""));
	}

	QStandardItem* statusItem = nullptr;
	QStandardItem* progressItem = nullptr;

	switch (status) {

	case ModelStatus::DOWNLOAD_BEGAN:
		statusItem = createItem("Downloading");
		statusItem->setForeground(QBrush(QColor(0, 111, 255)));
		progressItem = createItem("0%");
		break;

	case ModelStatus::DOWNLOAD_ENDED:
		statusItem = createItem("Downloaded");
		statusItem->setForeground(QBrush(QColor(0, 111, 255)));
		progressItem = createItem("Finished");
		break;

	case ModelStatus::DOWNLOADING:
		statusItem = createItem("Downloading");
		statusItem->setForeground(QBrush(QColor(0, 111, 255)));
		progressItem = createItem(QString("%1%").arg(progress * 100, 0, 'f', 2));
		break;

	case ModelStatus::ACTIVE:
		statusItem = createItem("Active");
		statusItem->setForeground(QBrush(QColor(38, 191, 107)));
		progressItem = createItem("");
		break;

	case ModelStatus::FAILED:
		statusItem = createItem("Failed");
		statusItem->setForeground(QBrush(QColor(240, 93, 93)));
		progressItem = createItem("");
		break;
	}

	if (statusItem) {
		m_modelTableItemModel->setItem(row, 2, statusItem);
	}

	if (progressItem) {
		m_modelTableItemModel->setItem(row, 3, progressItem);
	}

	m_modelTableItemModel->setItem(row, 4, createItem(model->inferenceRate()));

	QModelIndex tl = m_modelTableItemModel->index(row, 0);
	QModelIndex br = m_modelTableItemModel->index(row, m_modelTableItemModel->columnCount() - 1);

	emit m_modelTableItemModel->dataChanged(tl, br);
}

//-----------------------------------------------------------------------------
// Private
//-----------------------------------------------------------------------------

void
ModelDock::setup() {

	ui->table->horizontalHeader()->setSectionResizeMode(QHeaderView::Stretch);

	m_modelTableItemModel = new QStandardItemModel(0, 5, this);
	m_modelTableItemModel->setHorizontalHeaderLabels({
		"Model",
		"Version",
		"Status",
		"Progress",
		"FPS"
	});

	ui->table->setModel(m_modelTableItemModel);
}

QStandardItem*
ModelDock::createItem(const QString& label) {
	auto item = new QStandardItem(label);
	item->setTextAlignment(Qt::AlignCenter);
	return item;
};

QStandardItem*
ModelDock::createItem(double label) {
	auto item = new QStandardItem(QString::number(label, 'f', 2));
	item->setTextAlignment(Qt::AlignCenter);
	return item;
}

QStandardItem*
ModelDock::createItem(float label) {
	auto item = new QStandardItem(QString::number(label, 'f', 2));
	item->setTextAlignment(Qt::AlignCenter);
	return item;
}
