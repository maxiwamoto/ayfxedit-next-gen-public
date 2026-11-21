#include "workshop_dialog.h"
class WaveGraph : public QWidget {
public:
    const ayfx::SourceWave* wave=nullptr;
    double start=0,end=0,viewStart=0,viewEnd=0;
    std::function<void(double,double)> selected;
    double anchor=0;
    QImage spectral;
    WaveGraph(){setMinimumHeight(180);}
    void updateSpectrum() {
        spectral={};if(!wave||wave->mono.empty())return;
        const int w=320,h=80;spectral=QImage(w,h,QImage::Format_RGB32);
        auto samples=ayfx::selectedSamples(*wave,0,wave->duration(),ayfx::SampleRate);
        for(int x=0;x<w;x++){auto power=ayfx::spectrum(samples,int(double(x)*samples.size()/w));for(int y=0;y<h;y++){
            int bin=std::clamp(int(std::exp(double(h-1-y)/h*std::log(double(power.size())))),0,int(power.size())-1);
            double level=std::clamp((10*std::log10(power[bin]+1e-12)+90)/90.,0.,1.);
            spectral.setPixelColor(x,y,QColor::fromHsvF(.68-level*.6,.8,level));
        }}update();
    }
protected:
    void paintEvent(QPaintEvent*) override {
        QPainter p(this);p.fillRect(rect(),QColor("#182735"));if(!wave||wave->mono.empty()){p.setPen(Qt::white);p.drawText(rect(),Qt::AlignCenter,"Open WAV or MP3 audio");return;}
        double range=std::max(.001,viewEnd-viewStart);p.drawImage(QRect(0,height()/2,width(),height()/2),spectral,QRectF(viewStart/wave->duration()*spectral.width(),0,range/wave->duration()*spectral.width(),spectral.height()));
        p.setPen(QColor("#50cfbb"));double middle=height()/4.;
        for(int x=0;x<width();x++){size_t a=std::min(wave->mono.size(),size_t((viewStart+double(x)*range/width())*wave->rate)),b=std::min(wave->mono.size(),size_t((viewStart+double(x+1)*range/width())*wave->rate)+1);float lo=0,hi=0;for(size_t i=a;i<b;i++){lo=std::min(lo,wave->mono[i]);hi=std::max(hi,wave->mono[i]);}p.drawLine(QPointF(x,middle-hi*middle),QPointF(x,middle-lo*middle));}
        p.fillRect(QRectF((start-viewStart)/range*width(),0,(end-start)/range*width(),height()),QColor(100,160,240,55));
    }
    void mousePressEvent(QMouseEvent* e) override {if(wave)anchor=viewStart+std::clamp(e->position().x()/width(),0.,1.)*(viewEnd-viewStart);}
    void mouseReleaseEvent(QMouseEvent* e) override {if(wave&&selected){auto end=viewStart+std::clamp(e->position().x()/width(),0.,1.)*(viewEnd-viewStart);if(std::abs(end-anchor)>.001)selected(std::min(anchor,end),std::max(anchor,end));}}
};
WorkshopDialog::WorkshopDialog(int hz,int fps,QWidget* parent,const ayfx::Effect* saved):QDialog(parent),clock(hz),rate(fps) {
    setWindowTitle("WAV / MP3 workshop");resize(820,680);
    auto layout=new QVBoxLayout(this);auto open=new QPushButton("Open WAV / MP3…");openAudio=open;layout->addWidget(open);connect(open,&QPushButton::clicked,this,[this]{loadAudio();});
    auto plot=new WaveGraph;graph=plot;layout->addWidget(plot);
    auto zoomRow=new QHBoxLayout;auto zoom=new QPushButton("Zoom selection");auto full=new QPushButton("Full audio");zoomRow->addWidget(zoom);zoomRow->addWidget(full);layout->addLayout(zoomRow);connect(zoom,&QPushButton::clicked,this,[this,plot]{if(!busy&&endTime->value()>startTime->value()){plot->viewStart=startTime->value();plot->viewEnd=endTime->value();plot->update();}});connect(full,&QPushButton::clicked,this,[this,plot]{plot->viewStart=0;plot->viewEnd=source.duration();plot->update();});
    auto form=new QFormLayout;layout->addLayout(form);
    startTime=realBox(0,0,300);endTime=realBox(0,0,300);channels=numberBox(2,1,3);
    normalize=new QCheckBox("Normalize");normalize->setChecked(true);
    attack=realBox(0,0,2000);decay=realBox(0,0,2000);brightness=realBox(0,-100,100);pitch=realBox(0,-24,24);tone=realBox(50,0,100);noise=realBox(50,0,100);
    auto pair=[&](QWidget* a,QWidget* b){auto w=new QWidget;auto row=new QHBoxLayout(w);row->setContentsMargins(0,0,0,0);row->addWidget(a);row->addWidget(b);return w;};
    form->addRow("Start / end (seconds)",pair(startTime,endTime));form->addRow("AY channels",pair(channels,normalize));
    form->addRow("Attack / decay (ms)",pair(attack,decay));form->addRow("Brightness / pitch (semitones)",pair(brightness,pitch));
    form->addRow("Tone / noise sensitivity",pair(tone,noise));
    auto buttons=new QHBoxLayout;layout->addLayout(buttons);
    convert=new QPushButton("Convert");original=new QPushButton("Original");resultButton=new QPushButton("AY result");apply=new QPushButton("Apply");auto stop=new QPushButton("Stop");auto close=new QPushButton("Close");
    for(auto b:{convert,original,resultButton,stop,apply,close})buttons->addWidget(b);
    status=new QLabel("Choose audio, then select a range. Drag on the waveform to trim.");status->setWordWrap(true);layout->addWidget(status);
    auto invalidate=[this,plot]{valid=false;apply->setEnabled(false);resultButton->setEnabled(false);plot->start=startTime->value();plot->end=endTime->value();plot->update();};
    for(auto b:{startTime,endTime,attack,decay,brightness,pitch,tone,noise})connect(b,QOverload<double>::of(&QDoubleSpinBox::valueChanged),this,[invalidate](double){invalidate();});
    connect(channels,QOverload<int>::of(&QSpinBox::valueChanged),this,[invalidate](int){invalidate();});connect(normalize,&QCheckBox::toggled,this,[invalidate](bool){invalidate();});
    plot->selected=[this](double a,double b){startTime->setValue(a);endTime->setValue(b);};
    connect(convert,&QPushButton::clicked,this,[this]{if(busy){*cancelled=true;return;}runConversion();});
    connect(stop,&QPushButton::clicked,this,[this]{playback.stop();previewOffset=0;});
    connect(close,&QPushButton::clicked,this,&WorkshopDialog::reject);
    connect(apply,&QPushButton::clicked,this,[this]{if(valid){playback.stop();accept();}});
    connect(original,&QPushButton::clicked,this,[this]{try{auto position=previewOffset+playback.position();previewOffset=position;auto wav=ayfx::pcmWave(ayfx::selectedSamples(source,startTime->value(),endTime->value(),ayfx::SampleRate));playback.wav(ayfx::seekWave(wav,position));}catch(const std::exception& e){showError(this,e);}});
    connect(resultButton,&QPushButton::clicked,this,[this]{try{previewOffset+=playback.position();playback.wav(ayfx::seekWave(resultWave,previewOffset));}catch(const std::exception& e){showError(this,e);}});
    connect(&watcher,&QFutureWatcher<ayfx::ConvertedWave>::finished,this,[this,open]{
        busy=false;graph->setEnabled(true);original->setEnabled(true);convert->setText("Convert");open->setEnabled(true);
        for(auto w:QList<QWidget*>{startTime,endTime,attack,decay,brightness,pitch,tone,noise,channels,normalize})w->setEnabled(true);
        try{result=watcher.result();if(*cancelled){status->setText("Conversion cancelled.");return;}resultWave=ayfx::convertedPreview(result,settings);originalWave=ayfx::pcmWave(ayfx::selectedSamples(source,settings.start,settings.end,ayfx::SampleRate));valid=true;apply->setEnabled(true);resultButton->setEnabled(true);status->setText(QString("Ready: %1 AY frames. Compare Original / AY result, then Apply.").arg(result.frames));}
        catch(const std::exception& e){status->setText(QString::fromUtf8(e.what()));}catch(...){status->setText("Conversion failed or was cancelled.");}
    });
    apply->setEnabled(false);resultButton->setEnabled(false);original->setEnabled(false);
    if(saved&&!saved->sourceAudio.empty()){setSource(ayfx::decodeSourceWave(saved->sourceAudio),textString(saved->sourcePath));ayfx::ConvertOptions o;std::istringstream recipe(saved->recipe);if(recipe>>o.channels>>o.attackMs>>o.decayMs>>o.brightness>>o.pitch>>o.normalize>>o.toneSensitivity>>o.noiseSensitivity){channels->setValue(o.channels);attack->setValue(o.attackMs);decay->setValue(o.decayMs);brightness->setValue(o.brightness);pitch->setValue(o.pitch);tone->setValue(o.toneSensitivity);noise->setValue(o.noiseSensitivity);normalize->setChecked(o.normalize);}}
}
void WorkshopDialog::setSource(ayfx::SourceWave wave,const QString& path) {
    playback.stop();previewOffset=0;valid=false;apply->setEnabled(false);resultButton->setEnabled(false);source=std::move(wave);sourceName=path;startTime->setValue(0);endTime->setValue(std::min(source.duration(),double(ayfx::MaxFrames)/rate));auto plot=static_cast<WaveGraph*>(graph);plot->wave=&source;plot->viewStart=0;plot->viewEnd=source.duration();plot->start=0;plot->end=endTime->value();plot->updateSpectrum();original->setEnabled(true);status->setText(QString("%1 — %2 seconds").arg(QFileInfo(path).fileName()).arg(source.duration(),0,'f',3));
}
void WorkshopDialog::loadAudio() {
    auto path=QFileDialog::getOpenFileName(this,"Open audio",{},"Audio (*.wav *.mp3)");if(path.isEmpty())return;
    try{ayfx::require(QFileInfo(path).size()<=64*1024*1024,"Audio exceeds 64 MB");if(path.endsWith(".wav",Qt::CaseInsensitive)){setSource(ayfx::decodeSourceWave(ayfx::read(filePath(path))),path);return;}
        status->setText("Decoding MP3…");openAudio->setEnabled(false);convert->setEnabled(false);new AudioImport(path,[this,path](ayfx::SourceWave w,QString error){openAudio->setEnabled(true);convert->setEnabled(true);if(!error.isEmpty())status->setText(error);else setSource(std::move(w),path);},this);
    }catch(const std::exception& e){showError(this,e);}
}
void WorkshopDialog::runConversion() {
    try{settings={};settings.start=startTime->value();settings.end=endTime->value();settings.channels=channels->value();settings.clock=clock;settings.rate=rate;settings.attackMs=attack->value();settings.decayMs=decay->value();settings.brightness=brightness->value();settings.pitch=pitch->value();settings.toneSensitivity=tone->value();settings.noiseSensitivity=noise->value();settings.normalize=normalize->isChecked();ayfx::validateConversion(source,settings);
        playback.stop();previewOffset=0;openAudio->setEnabled(false);graph->setEnabled(false);original->setEnabled(false);*cancelled=false;busy=true;valid=false;apply->setEnabled(false);resultButton->setEnabled(false);convert->setText("Cancel conversion");status->setText("Converting…");
        for(auto w:QList<QWidget*>{startTime,endTime,attack,decay,brightness,pitch,tone,noise,channels,normalize})w->setEnabled(false);
        auto input=source;auto options=settings;auto cancel=cancelled;
        watcher.setFuture(QtConcurrent::run([input=std::move(input),options,cancel]{return ayfx::convertWave(input,options,[cancel]{return cancel->load();});}));
    }catch(const std::exception& e){showError(this,e);}
}
std::string WorkshopDialog::recipe() const {
    std::ostringstream out;out<<channels->value()<<' '<<attack->value()<<' '<<decay->value()<<' '<<brightness->value()<<' '<<pitch->value()<<' '<<normalize->isChecked()<<' '<<tone->value()<<' '<<noise->value();return out.str();
}
void WorkshopDialog::reject(){if(busy){*cancelled=true;status->setText("Cancelling…");return;}playback.stop();QDialog::reject();}
WorkshopDialog::~WorkshopDialog(){*cancelled=true;watcher.waitForFinished();}
