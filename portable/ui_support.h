#pragma once
#include <QtWidgets>
#include <QAudioSink>
#include <QAudioDevice>
#include <QMediaDevices>
#include <QAudioDecoder>
#include <QAudioBuffer>
#include <QtConcurrent>
#include "sound_tools.h"
#include <atomic>

inline std::filesystem::path filePath(const QString& s) {
#ifdef _WIN32
    return std::filesystem::path(s.toStdWString());
#else
    return std::filesystem::u8path(s.toUtf8().constData());
#endif
}
inline QString textString(const std::string& s) { return QString::fromUtf8(s.data(), int(s.size())); }
inline QString pathString(const std::filesystem::path& p) {
#ifdef _WIN32
    return QString::fromStdWString(p.wstring());
#else
    return textString(p.u8string());
#endif
}
inline void showError(QWidget* owner, const std::exception& e) {
    QMessageBox::warning(owner, "AYFXEDIT NEXT GEN", QString::fromUtf8(e.what()));
}
inline QSpinBox* numberBox(int value, int minimum, int maximum, QWidget* parent=nullptr) {
    auto box=new QSpinBox(parent); box->setRange(minimum,maximum);box->setValue(value);return box;
}
inline QDoubleSpinBox* realBox(double value,double minimum,double maximum,QWidget* parent=nullptr) {
    auto box=new QDoubleSpinBox(parent);box->setDecimals(3);box->setRange(minimum,maximum);box->setValue(value);return box;
}
class AYDevice : public QIODevice {
    std::unique_ptr<ayfx::AYStream> stream;
    qint64 remaining;
public:
    AYDevice(std::unique_ptr<ayfx::AYStream> data,qint64 samples):stream(std::move(data)),remaining(samples) {open(ReadOnly);}
    bool isSequential() const override {return true;}
    qint64 bytesAvailable() const override {return (remaining<0?8192:remaining*2)+QIODevice::bytesAvailable();}
    bool atEnd() const override {return remaining==0;}
    qint64 readData(char* out,qint64 count) override {
        auto samples=count/2;if(remaining>=0)samples=std::min(samples,remaining);
        for(qint64 i=0;i<samples;i++){auto value=uint16_t(stream->next());out[i*2]=char(value);out[i*2+1]=char(value>>8);}
        if(remaining>=0)remaining-=samples;
        return samples*2;
    }
    qint64 writeData(const char*,qint64) override {return -1;}
};
class Playback {
    std::unique_ptr<QAudioSink> sink;
    std::unique_ptr<QIODevice> input;
public:
    ~Playback(){stop();}
    void stop(){if(sink)sink->stop();sink.reset();input.reset();}
    void start(std::unique_ptr<QIODevice> data) {
        stop();QAudioFormat format;format.setSampleRate(ayfx::SampleRate);format.setChannelCount(1);format.setSampleFormat(QAudioFormat::Int16);
        auto device=QMediaDevices::defaultAudioOutput();
        ayfx::require(!device.isNull(),"No audio output device is available");
        ayfx::require(device.isFormatSupported(format),"Audio device does not support 44.1 kHz mono 16-bit playback");
        input=std::move(data);sink=std::make_unique<QAudioSink>(device,format);sink->setBufferSize(16384);sink->start(input.get());
        ayfx::require(sink->error()==QAudio::NoError,"Cannot start audio output");
    }
    void wav(const ayfx::Bytes& bytes) {
        ayfx::require(bytes.size()>=44,"Invalid preview audio");
        auto buffer=std::make_unique<QBuffer>();buffer->setData(QByteArray(reinterpret_cast<const char*>(bytes.data()+44),int(bytes.size()-44)));buffer->open(QIODevice::ReadOnly);start(std::move(buffer));
    }
    double position() const {return sink?sink->processedUSecs()/1000000.:0.;}
};

class AudioImport : public QObject {
    QAudioDecoder decoder;
    ayfx::SourceWave wave;
    QTimer timeout;
    std::function<void(ayfx::SourceWave,QString)> done;
    bool complete=false;
    void finish(QString error={}) {
        if(complete)return;complete=true;timeout.stop();decoder.stop();
        if(error.isEmpty()&&wave.mono.empty())error="No audio samples were decoded.";
        done(std::move(wave),error);deleteLater();
    }
public:
    AudioImport(QString path,std::function<void(ayfx::SourceWave,QString)> callback,QObject* parent):QObject(parent),done(std::move(callback)) {
        QAudioFormat format;format.setSampleFormat(QAudioFormat::Int16);decoder.setAudioFormat(format);
        connect(&decoder,&QAudioDecoder::bufferReady,this,[this]{
            try {
                auto b=decoder.read();auto fmt=b.format();
                ayfx::require(b.isValid()&&fmt.sampleFormat()==QAudioFormat::Int16,"Unsupported decoded audio format");
                if(!wave.rate){wave.rate=fmt.sampleRate();wave.channels=fmt.channelCount();wave.bits=16;}
                ayfx::require(wave.rate==fmt.sampleRate()&&wave.channels==fmt.channelCount()&&wave.channels>=1&&wave.channels<=2,"Audio format changed or has more than two channels");
                ayfx::require(wave.rate>=4000&&wave.rate<=192000,"Unsupported audio sample rate");
                ayfx::require(wave.mono.size()+b.frameCount()<=size_t(wave.rate)*300,"Audio exceeds five minutes");
                auto samples=b.constData<qint16>();
                for(int f=0;f<b.frameCount();f++){double sum=0;for(int c=0;c<wave.channels;c++)sum+=samples[f*wave.channels+c]/32768.;wave.mono.push_back(float(sum/wave.channels));}
            }catch(const std::exception& e){finish(QString::fromUtf8(e.what()));}
        });
        connect(&decoder,&QAudioDecoder::finished,this,[this]{finish();});
        connect(&decoder,QOverload<QAudioDecoder::Error>::of(&QAudioDecoder::error),this,[this](QAudioDecoder::Error){finish(decoder.errorString());});
        timeout.setSingleShot(true);connect(&timeout,&QTimer::timeout,this,[this]{finish("Audio decoding timed out.");});
        decoder.setSource(QUrl::fromLocalFile(QFileInfo(path).absoluteFilePath()));timeout.start(120000);decoder.start();
    }
};
