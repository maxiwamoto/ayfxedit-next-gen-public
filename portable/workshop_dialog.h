#pragma once
#include "ui_support.h"
class WorkshopDialog : public QDialog {
    ayfx::SourceWave source;
    QString sourceName;
    std::shared_ptr<std::atomic_bool> cancelled=std::make_shared<std::atomic_bool>(false);
    QFutureWatcher<ayfx::ConvertedWave> watcher;
    QDoubleSpinBox *startTime,*endTime,*attack,*decay,*brightness,*pitch,*tone,*noise;
    QSpinBox* channels;
    QCheckBox* normalize;
    QLabel *status;
    QWidget* graph;
    QPushButton *openAudio,*convert,*apply,*original,*resultButton;
    ayfx::ConvertOptions settings;
    ayfx::ConvertedWave result;
    ayfx::Bytes originalWave, resultWave;
    Playback playback;
    bool busy=false,valid=false;
    double previewOffset=0;
    int clock,rate;
    void loadAudio();
    void setSource(ayfx::SourceWave wave,const QString& path);
    void runConversion();
public:
    explicit WorkshopDialog(int clock,int rate,QWidget* parent=nullptr,const ayfx::Effect* saved=nullptr);
    ~WorkshopDialog();
    ayfx::ConvertedWave output() const {return result;}
    ayfx::Bytes sourceExcerpt() const {return originalWave;}
    std::string sourcePath() const {return sourceName.toUtf8().toStdString();}
    std::string recipe() const;
    void reject() override;
};
