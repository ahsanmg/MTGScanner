#include <QtTest/QtTest>

#include <output/nameplatemodel.h>

namespace {

QImage makeImage(int value)
{
    QImage image(4, 4, QImage::Format_ARGB32);
    image.fill(QColor(value, value, value));
    return image;
}

}

class NameplateModelTest : public QObject
{
    Q_OBJECT

private slots:
    void startsEmpty();
    void prependsNewNameplates();
    void exposesImageAndSizeRoles();
    void replacesExistingTrackerImage();
    void evictsOldEntriesWithoutLosingIdAlignment();
};

void NameplateModelTest::startsEmpty()
{
    MTGS::NameplateModel model;

    QCOMPARE(model.rowCount(QModelIndex{}), 0);
    QVERIFY(!model.data(QModelIndex{}, Qt::DecorationRole).isValid());
    QVERIFY(!model.data(model.index(-1, 0), Qt::DecorationRole).isValid());
}

void NameplateModelTest::prependsNewNameplates()
{
    MTGS::NameplateModel model;
    const QImage first = makeImage(10);
    const QImage second = makeImage(20);

    model.addNameplate(1, first);
    model.addNameplate(2, second);

    QCOMPARE(model.rowCount(QModelIndex{}), 2);
    QCOMPARE(model.data(model.index(0, 0), Qt::DecorationRole).value<QImage>(), second);
    QCOMPARE(model.data(model.index(1, 0), Qt::DecorationRole).value<QImage>(), first);
}

void NameplateModelTest::exposesImageAndSizeRoles()
{
    MTGS::NameplateModel model;
    model.addNameplate(1, makeImage(10));

    QCOMPARE(model.data(model.index(0, 0), Qt::SizeHintRole).toSize(), QSize(312, 44));
    QVERIFY(!model.data(model.index(0, 0), Qt::DisplayRole).isValid());
}

void NameplateModelTest::replacesExistingTrackerImage()
{
    MTGS::NameplateModel model;
    const QImage original = makeImage(10);
    const QImage replacement = makeImage(20);
    model.addNameplate(1, original);

    model.addNameplate(1, replacement);

    QCOMPARE(model.rowCount(QModelIndex{}), 1);
    QCOMPARE(model.data(model.index(0, 0), Qt::DecorationRole).value<QImage>(), replacement);
}

void NameplateModelTest::evictsOldEntriesWithoutLosingIdAlignment()
{
    MTGS::NameplateModel model(2);
    for (size_t id = 1; id <= 4; ++id)
        model.addNameplate(id, makeImage(static_cast<int>(id)));

    model.addNameplate(5, makeImage(5));
    QCOMPARE(model.rowCount(QModelIndex{}), 2);

    // ID 2 was evicted and must be treated as a new entry, not as an old index.
    model.addNameplate(2, makeImage(20));

    QCOMPARE(model.rowCount(QModelIndex{}), 3);
    QCOMPARE(model.data(model.index(0, 0), Qt::DecorationRole).value<QImage>(), makeImage(20));
}

QTEST_MAIN(NameplateModelTest)
#include "test_nameplatemodel.moc"