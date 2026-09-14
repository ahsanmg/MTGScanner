#include <QtTest/QtTest>

#include <QImage>
#include <QPainter>
#include <QStandardItemModel>

#include <core/constants.hpp>
#include <output/nameplatdelegate.h>

class NameplateDelegateTest : public QObject
{
    Q_OBJECT

private slots:
    void usesDefaultSizeHint();
    void usesModelSizeHint();
    void paintsNameplateImage();
};

void NameplateDelegateTest::usesDefaultSizeHint()
{
    MTGS::NameplateDelegate delegate;
    QStandardItemModel model(1, 1);
    const QModelIndex index = model.index(0, 0);
    QStyleOptionViewItem option;

    QCOMPARE(delegate.sizeHint(option, index), QSize(MTGS::TITLE_WIDTH, MTGS::TITLE_HEIGHT + 6));
}

void NameplateDelegateTest::usesModelSizeHint()
{
    MTGS::NameplateDelegate delegate;
    QStandardItemModel model(1, 1);
    const QModelIndex index = model.index(0, 0);
    model.setData(index, QSize(123, 45), Qt::SizeHintRole);
    QStyleOptionViewItem option;

    QCOMPARE(delegate.sizeHint(option, index), QSize(123, 45));
}

void NameplateDelegateTest::paintsNameplateImage()
{
    MTGS::NameplateDelegate delegate;
    QStandardItemModel model(1, 1);
    const QModelIndex index = model.index(0, 0);
    QImage source(8, 8, QImage::Format_ARGB32);
    source.fill(Qt::red);
    model.setData(index, source, Qt::DecorationRole);

    QImage target(40, 40, QImage::Format_ARGB32);
    target.fill(Qt::transparent);
    QPainter painter(&target);
    QStyleOptionViewItem option;
    option.rect = QRect(0, 0, 40, 40);
    delegate.paint(&painter, option, index);
    painter.end();

    QVERIFY(target.pixelColor(20, 20).red() > 200);
    QVERIFY(target.pixelColor(20, 20).alpha() > 0);
}

QTEST_MAIN(NameplateDelegateTest)
#include "test_nameplatdelegate.moc"