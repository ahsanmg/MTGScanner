#include "nameplatemodel.h"

#include <cstddef>

#include <QSize>
#include <QDebug>
#include <QImage>
#include <QVariant>
#include <QAbstractItemModel>

#include <opencv2/core/mat.hpp>

namespace MTGS {

NameplateModel::NameplateModel(int maxNameplates, QObject *parent)
    : QAbstractListModel(parent)
    , m_maxNameplates(maxNameplates)
{}

int NameplateModel::rowCount(const QModelIndex &parent) const
{
    return m_nameplates.size();
}

QVariant NameplateModel::data(const QModelIndex &index, int role) const
{
    if (!index.isValid()
        || index.row() < 0
        || index.row() >= m_nameplates.size())
        return QVariant();

    if (role == Qt::DecorationRole)
        return QVariant::fromValue(m_nameplates.at(index.row()));

    if (role == Qt::SizeHintRole)
        return QSize(312, 44);

    return QVariant();
}

void NameplateModel::addNameplate(size_t trackedCardId, QImage image)
{
    if (m_nameplates.size() >= m_maxNameplates * 2) {
        int first = m_maxNameplates - 1;
        int last = m_nameplates.size() - 1;
        beginRemoveRows(QModelIndex(), first, last);
        m_nameplates.remove(first, m_nameplates.size() - first);
        m_trackedCardIds.remove(first, m_nameplates.size() - first);
        endRemoveRows();
    }

    size_t indx = m_trackedCardIds.indexOf(trackedCardId);
    if (indx != -1) {
        m_nameplates[indx] = image;
        emit dataChanged(createIndex(indx, 0), createIndex(indx, 0), { Qt::DecorationRole });
    } else {
        beginInsertRows(QModelIndex(), 0, 0);    
        m_nameplates.prepend(image); // we always prepend
        m_trackedCardIds.prepend(trackedCardId);
        endInsertRows();
    }
}

}