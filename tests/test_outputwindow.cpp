#include <QtTest/QtTest>

#include <output/nameplatemodel.h>
#include <output/outputwindow.h>

class OutputWindowTest : public QObject
{
    Q_OBJECT

private slots:
    void startsWithoutModel();
    void installsModel();
    void replacesModel();
};

void OutputWindowTest::startsWithoutModel()
{
    MTGS::OutputWindow window;

    QCOMPARE(window.model(), nullptr);
}

void OutputWindowTest::installsModel()
{
    MTGS::NameplateModel model;
    MTGS::OutputWindow window(true, true, &model);

    QCOMPARE(window.model(), &model);
}

void OutputWindowTest::replacesModel()
{
    MTGS::NameplateModel first;
    MTGS::NameplateModel second;
    MTGS::OutputWindow window(true, true, &first);

    window.setModel(&second);

    QCOMPARE(window.model(), &second);
}

QTEST_MAIN(OutputWindowTest)
#include "test_outputwindow.moc"