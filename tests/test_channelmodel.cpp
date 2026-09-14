#include <QtTest/QtTest>

#include <engine/channelmodel.h>

namespace {

MTGS::ChannelOptions makeOptions(const QString &id, const QString &name)
{
    MTGS::ChannelOptions options;
    options.id = id;
    options.name = name;
    options.windowName = name + QStringLiteral(" window");
    options.windowGeometry = QRect(10, 20, 640, 480);
    return options;
}

}

class ChannelModelTest : public QObject
{
    Q_OBJECT

private slots:
    void startsEmpty();
    void exposesExpectedRoles();
    void addsChannelsInOrder();
    void deletesMatchingChannel();
    void ignoresMissingChannel();
};

void ChannelModelTest::startsEmpty()
{
    MTGS::ChannelModel model;

    QCOMPARE(model.rowCount(QModelIndex{}), 0);
    QVERIFY(!model.data(QModelIndex{}, MTGS::ChannelModel::IdRole).isValid());
}

void ChannelModelTest::exposesExpectedRoles()
{
    MTGS::ChannelModel model;
    const auto options = makeOptions(QStringLiteral("camera-1"), QStringLiteral("Desk camera"));
    model.channelAdded(options);

    const QModelIndex index = model.index(0, 0);
    QCOMPARE(model.data(index, MTGS::ChannelModel::IdRole).toString(), options.id);
    QCOMPARE(model.data(index, MTGS::ChannelModel::NameRole).toString(), options.name);
    QCOMPARE(model.data(index, MTGS::ChannelModel::DeviceRole).value<QCameraDevice>(), options.cameraDevice);
    QCOMPARE(model.roleNames().value(MTGS::ChannelModel::IdRole), QByteArrayLiteral("id"));
    QCOMPARE(model.roleNames().value(MTGS::ChannelModel::NameRole), QByteArrayLiteral("name"));
    QCOMPARE(model.roleNames().value(MTGS::ChannelModel::DeviceRole), QByteArrayLiteral("device"));
}

void ChannelModelTest::addsChannelsInOrder()
{
    MTGS::ChannelModel model;
    const auto first = makeOptions(QStringLiteral("camera-1"), QStringLiteral("First"));
    const auto second = makeOptions(QStringLiteral("camera-2"), QStringLiteral("Second"));

    model.channelAdded(first);
    model.channelAdded(second);

    QCOMPARE(model.rowCount(QModelIndex{}), 2);
    QCOMPARE(model.data(model.index(0, 0), MTGS::ChannelModel::NameRole).toString(), first.name);
    QCOMPARE(model.data(model.index(1, 0), MTGS::ChannelModel::NameRole).toString(), second.name);
}

void ChannelModelTest::deletesMatchingChannel()
{
    MTGS::ChannelModel model;
    const auto first = makeOptions(QStringLiteral("camera-1"), QStringLiteral("First"));
    const auto second = makeOptions(QStringLiteral("camera-2"), QStringLiteral("Second"));
    model.channelAdded(first);
    model.channelAdded(second);

    model.channelDeleted(first);

    QCOMPARE(model.rowCount(QModelIndex{}), 1);
    QCOMPARE(model.data(model.index(0, 0), MTGS::ChannelModel::IdRole).toString(), second.id);
}

void ChannelModelTest::ignoresMissingChannel()
{
    MTGS::ChannelModel model;
    const auto existing = makeOptions(QStringLiteral("camera-1"), QStringLiteral("Existing"));
    const auto missing = makeOptions(QStringLiteral("camera-2"), QStringLiteral("Missing"));
    model.channelAdded(existing);

    model.channelDeleted(missing);

    QCOMPARE(model.rowCount(QModelIndex{}), 1);
    QCOMPARE(model.data(model.index(0, 0), MTGS::ChannelModel::IdRole).toString(), existing.id);
}

QTEST_MAIN(ChannelModelTest)
#include "test_channelmodel.moc"