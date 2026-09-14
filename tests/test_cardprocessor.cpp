#include <QtTest/QtTest>

#include <QImage>

#include <core/constants.hpp>
#include <core/frame.hpp>
#include <engine/cardprocessor.h>

namespace {

MTGS::Prediction makeTitle()
{
    MTGS::Prediction title;
    title.box = QRect(0, 0, 10, 50);
    title.className = QStringLiteral("title");
    title.classId = 1;
    title.confidence = 0.95f;
    title.keypoints = {
        {{0.0, 0.0}, 1.0f, 0},
        {{10.0, 0.0}, 1.0f, 1},
        {{10.0, 50.0}, 1.0f, 2},
        {{0.0, 50.0}, 1.0f, 3}
    };
    return title;
}

MTGS::Prediction makeCard()
{
    MTGS::Prediction card;
    card.box = QRect(0, 0, 100, 100);
    card.className = QStringLiteral("card_front");
    card.classId = 0;
    card.confidence = 0.95f;
    return card;
}

}

class CardProcessorTest : public QObject
{
    Q_OBJECT
private slots:
    void processesEmptyFrame();
    void relatesAndCropsTallTitleInsideCard();
};

void CardProcessorTest::processesEmptyFrame()
{
    MTGS::CardProcessor processor;
    MTGS::FramePtr frame = MTGS::FramePtr::create();

    processor.process(frame);

    QVERIFY(frame->predictions.isEmpty());
}

void CardProcessorTest::relatesAndCropsTallTitleInsideCard()
{
    MTGS::CardProcessor processor;
    MTGS::FramePtr frame = MTGS::FramePtr::create();
    frame->frameImg = QImage(200, 200, QImage::Format_RGB888);
    frame->frameImg.fill(Qt::blue);
    frame->predictions = { makeCard(), makeTitle() };

    processor.process(frame);

    QCOMPARE(frame->predictions.size(), 1);
    const auto &card = frame->predictions.first();
    QVERIFY(card.subPredictions.has_value());
    QCOMPARE(card.subPredictions->size(), 1);
    QVERIFY(card.crops.has_value());
    QCOMPARE(card.crops->size(), 1);
    QCOMPARE(card.crops->first().size(), QSize(MTGS::TITLE_WIDTH, MTGS::TITLE_HEIGHT));
}

QTEST_MAIN(CardProcessorTest)
#include "test_cardprocessor.moc"