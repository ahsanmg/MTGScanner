#include <QtTest/QtTest>

#include <QSignalSpy>

#include <camera/cameramanager.h>

class CameraManagerTest : public QObject
{
    Q_OBJECT

private slots:
    void startsWithQtVideoInputs();
    void findsCamerasByIdOrDescription();
    void tracksInUseCameras();
};

void CameraManagerTest::startsWithQtVideoInputs()
{
    MTGS::CameraManager manager;

    const QList<QCameraDevice> expected = QMediaDevices::videoInputs();
    QCOMPARE(manager.availableCameras(), expected);
    for (const auto &camera : expected)
        QVERIFY(!manager.isCameraInUse(camera));
}

void CameraManagerTest::findsCamerasByIdOrDescription()
{
    MTGS::CameraManager manager;

    QVERIFY(!manager.findBy(QStringLiteral("missing-id"), QStringLiteral("missing-description")));
    for (const auto &camera : manager.availableCameras()) {
        const auto by_id = manager.findBy(camera.id(), QStringLiteral("missing-description"));
        QVERIFY(by_id.has_value());
        QCOMPARE(by_id->id(), camera.id());

        const auto by_description = manager.findBy(QStringLiteral("missing-id"), camera.description());
        QVERIFY(by_description.has_value());
        QCOMPARE(by_description->description(), camera.description());
    }
}

void CameraManagerTest::tracksInUseCameras()
{
    MTGS::CameraManager manager;
    const auto cameras = manager.availableCameras();

    for (const auto &camera : cameras) {
        QSignalSpy status_spy(&manager, &MTGS::CameraManager::cameraStatusChanged);
        QSignalSpy available_spy(&manager, &MTGS::CameraManager::availableCamerasChanged);

        manager.setCameraInUse(camera, true);

        QVERIFY(manager.isCameraInUse(camera));
        QVERIFY(!manager.availableCameras().contains(camera));
        QCOMPARE(status_spy.count(), 1);
        QCOMPARE(available_spy.count(), 1);
        QCOMPARE(status_spy.at(0).at(1).toBool(), true);

        manager.setCameraInUse(camera, false);

        QVERIFY(!manager.isCameraInUse(camera));
        QVERIFY(manager.availableCameras().contains(camera));
        QCOMPARE(status_spy.count(), 2);
        QCOMPARE(available_spy.count(), 2);
        QCOMPARE(status_spy.at(1).at(1).toBool(), false);
    }
}

QTEST_MAIN(CameraManagerTest)
#include "test_cameramanager.moc"