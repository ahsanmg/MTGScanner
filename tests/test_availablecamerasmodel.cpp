#include <QtTest/QtTest>

#include <QSignalSpy>

#include <camera/availablecamerasmodel.h>
#include <camera/cameramanager.h>

class AvailableCamerasModelTest : public QObject
{
    Q_OBJECT

private slots:
    void ignoresNullManager();
    void mirrorsManagerCameras();
    void exposesCameraRoles();
};

void AvailableCamerasModelTest::ignoresNullManager()
{
    MTGS::AvailableCamerasModel model;

    model.setCameraManager(nullptr);

    QCOMPARE(model.rowCount(QModelIndex{}), 0);
    QVERIFY(!model.data(QModelIndex{}, MTGS::AvailableCamerasModel::Id).isValid());
}

void AvailableCamerasModelTest::mirrorsManagerCameras()
{
    MTGS::CameraManager manager;
    MTGS::AvailableCamerasModel model;
    QSignalSpy resetSpy(&model, &QAbstractItemModel::modelReset);

    model.setCameraManager(&manager);

    QCOMPARE(model.rowCount(QModelIndex{}), manager.availableCameras().size());
    QVERIFY(resetSpy.count() >= 1);

    manager.onVideoInputsChanged();
    QCOMPARE(model.rowCount(QModelIndex{}), manager.availableCameras().size());
}

void AvailableCamerasModelTest::exposesCameraRoles()
{
    MTGS::CameraManager manager;
    MTGS::AvailableCamerasModel model;
    model.setCameraManager(&manager);

    const auto roles = model.roleNames();
    QCOMPARE(roles.value(MTGS::AvailableCamerasModel::Id), QByteArrayLiteral("id"));
    QCOMPARE(roles.value(MTGS::AvailableCamerasModel::Description), QByteArrayLiteral("description"));
    QCOMPARE(roles.value(MTGS::AvailableCamerasModel::IsDefault), QByteArrayLiteral("isdefault"));
    QCOMPARE(roles.value(MTGS::AvailableCamerasModel::IsNull), QByteArrayLiteral("isnull"));
    QCOMPARE(roles.value(MTGS::AvailableCamerasModel::PhotoResolutions), QByteArrayLiteral("photores"));
    QCOMPARE(roles.value(MTGS::AvailableCamerasModel::Position), QByteArrayLiteral("pos"));
    QCOMPARE(roles.value(MTGS::AvailableCamerasModel::VideoFormats), QByteArrayLiteral("vidformats"));
    QCOMPARE(roles.value(MTGS::AvailableCamerasModel::Device), QByteArrayLiteral("device"));

    for (int row = 0; row < model.rowCount(QModelIndex{}); ++row) {
        const QModelIndex index = model.index(row, 0);
        const QCameraDevice camera = model.data(index, MTGS::AvailableCamerasModel::Device).value<QCameraDevice>();
        QCOMPARE(model.data(index, MTGS::AvailableCamerasModel::Id).toByteArray(), camera.id());
        QCOMPARE(model.data(index, MTGS::AvailableCamerasModel::Description).toString(), camera.description());
        QCOMPARE(model.data(index, MTGS::AvailableCamerasModel::IsDefault).toBool(), camera.isDefault());
        QCOMPARE(model.data(index, MTGS::AvailableCamerasModel::IsNull).toBool(), camera.isNull());
    }

    QVERIFY(!model.data(model.index(model.rowCount(QModelIndex{}), 0), MTGS::AvailableCamerasModel::Id).isValid());
}

QTEST_MAIN(AvailableCamerasModelTest)
#include "test_availablecamerasmodel.moc"