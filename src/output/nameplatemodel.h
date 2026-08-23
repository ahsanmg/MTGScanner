#pragma once

#include <cstddef>

#include <QList>
#include <QImage>
#include <QObject>
#include <QAbstractListModel>

#include <opencv2/core/mat.hpp>

namespace MTGS {

class NameplateModel : public QAbstractListModel {
    Q_OBJECT
public:
    explicit NameplateModel(int maxNameplates = 10, QObject *parent = nullptr);

    // QAbstractItemModel interface
    int rowCount(const QModelIndex &parent) const override;
    QVariant data(const QModelIndex &index, int role) const override;

public slots:
    void addNameplate(size_t trackedCardId, QImage image);

private:
    QList<size_t> m_trackedCardIds;
    QList<QImage> m_nameplates;
    int m_maxNameplates = 10;
};

}