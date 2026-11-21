#include "ui_support.h"
#include "workshop_dialog.h"
#include "game_export.h"
#include "register_export.h"
#include "command_ids.h"
#include "help_content.h"
#include <deque>
#include <iostream>
using namespace ayfx;
namespace fs=std::filesystem;
struct Document {
    Bank bank;
    Project project;
    Document():bank(1),project{groupBank(bank),1789773,50} {}
    int group=0;
    bool dirty=false;
    fs::path path;
};
class FrameModel : public QAbstractTableModel {
public:
    Document* doc;
    std::function<void()> beforeEdit,afterEdit;
    bool gesture=false;
    explicit FrameModel(Document* d,QObject* parent):QAbstractTableModel(parent),doc(d){}
    int rowCount(const QModelIndex& ={}) const override {return MaxFrames;}
    int columnCount(const QModelIndex& ={}) const override {return 24;}
    int track(int column) const {return doc->project.groups[doc->group].tracks[column/8];}
    void reset(){beginResetModel();endResetModel();}
    QVariant headerData(int section,Qt::Orientation orientation,int role) const override {
        if(role!=Qt::DisplayRole)return {};
        if(orientation==Qt::Vertical)return QString("%1").arg(section,4,16,QChar('0')).toUpper();
        const char* names[]={"T","N","Tone","Noise","Vol","Env","Period","Shape"};
        return QString("%1\n%2").arg(QChar('A'+section/8)).arg(names[section%8]);
    }
    QVariant data(const QModelIndex& i,int role) const override {
        if(!i.isValid())return {};auto t=track(i.column());if(t<0)return {};auto c=doc->bank[t].cells[i.row()];int field=i.column()%8;
        if(role==Qt::CheckStateRole&&(field==0||field==1||field==5))return (field==0?c.t:field==1?c.n:c.envelope)?Qt::Checked:Qt::Unchecked;
        if(role==Qt::DisplayRole||role==Qt::EditRole){
            if(field==0||field==1||field==5)return {};
            int v=field==2?c.tone:field==3?c.noise:field==4?c.volume:field==6?c.envelopePeriod:c.shape;
            if(v<0)return QString("-");return QString::number(v,16).toUpper();
        }
        if(role==Qt::TextAlignmentRole)return int(Qt::AlignCenter);
        if(role==Qt::BackgroundRole){
            auto& g=doc->project.groups[doc->group];
            if(noiseConflict(doc->bank,g,i.row()))return QColor("#f8c2bd");
            if(i.row()==g.loopStart||i.row()==g.loopEnd)return QColor("#dac4ef");
            if(field==4)return QColor::fromRgbF(.9-c.volume*.025,.97,.94-c.volume*.015);
        }
        return {};
    }
    Qt::ItemFlags flags(const QModelIndex& i) const override {
        if(!i.isValid()||track(i.column())<0)return Qt::NoItemFlags;
        auto f=Qt::ItemIsEnabled|Qt::ItemIsSelectable;int field=i.column()%8;
        return f|((field==0||field==1||field==5)?Qt::ItemIsUserCheckable:Qt::ItemIsEditable);
    }
    bool setData(const QModelIndex& i,const QVariant& v,int role) override {
        if(!i.isValid()||track(i.column())<0)return false;int field=i.column()%8;bool boolean=field==0||field==1||field==5;
        if((boolean&&role!=Qt::CheckStateRole)||(!boolean&&role!=Qt::EditRole))return false;
        bool ok=true;int value=boolean?v.toInt()==Qt::Checked:v.toString().toInt(&ok,16);
        if(!boolean&&field==7&&v.toString()=="-"){value=-1;ok=true;}
        int maximum=field==2?4095:field==3?31:field==6?65535:15;
        if(!ok||(!boolean&&(value<(field==7?-1:0)||value>maximum)))return false;
        if(beforeEdit&&!gesture)beforeEdit();auto& c=doc->bank[track(i.column())].cells[i.row()];
        switch(field){case 0:c.t=value;break;case 1:c.n=value;break;case 2:c.tone=value;break;case 3:c.noise=value;break;case 4:c.volume=value;c.envelope=false;break;case 5:c.envelope=value;break;case 6:c.envelopePeriod=value;break;case 7:c.shape=value;break;}
        emit dataChanged(index(i.row(),0),index(i.row(),23));if(afterEdit)afterEdit();return true;
    }
};
class ParameterDelegate : public QStyledItemDelegate {
public:
    using QStyledItemDelegate::QStyledItemDelegate;
    void paint(QPainter* p,const QStyleOptionViewItem& opt,const QModelIndex& i)const override {
        QStyledItemDelegate::paint(p,opt,i);
        int field=i.column()%8;if(field<2||field>4)return;
        bool ok=false;int value=i.data(Qt::DisplayRole).toString().toInt(&ok,16);if(!ok)return;
        int maximum=field==2?4095:field==3?31:15;
        p->fillRect(QRect(opt.rect.left()+2,opt.rect.bottom()-4,(opt.rect.width()-4)*value/maximum,3),QColor("#278b80"));
    }
};
class FrameView : public QTableView {
    bool drawing=false;
    void drawAt(const QPoint& pos) {
        auto i=indexAt(pos);if(!i.isValid())return;int field=i.column()%8;if(field<2||field>4)return;
        auto m=static_cast<FrameModel*>(model());if(m->track(i.column())<0)return;
        int max=field==2?4095:field==3?31:15;auto rect=visualRect(i);
        int value=std::clamp(int(double(pos.x()-rect.left())/std::max(1,rect.width()-1)*max),0,max);
        m->setData(i,QString::number(value,16),Qt::EditRole);
    }
protected:
    void mousePressEvent(QMouseEvent* e)override {
        if(e->button()==Qt::LeftButton&&(e->modifiers()&Qt::AltModifier)){
            auto i=indexAt(e->position().toPoint());auto m=static_cast<FrameModel*>(model());
            if(i.isValid()&&m->track(i.column())>=0&&i.column()%8>=2&&i.column()%8<=4){
                setCurrentIndex(i);if(m->beforeEdit)m->beforeEdit();m->gesture=true;drawing=true;drawAt(e->position().toPoint());return;
            }
        }QTableView::mousePressEvent(e);
    }
    void mouseMoveEvent(QMouseEvent* e)override {if(drawing){drawAt(e->position().toPoint());return;}QTableView::mouseMoveEvent(e);}
    void mouseReleaseEvent(QMouseEvent* e)override {if(drawing){drawing=false;static_cast<FrameModel*>(model())->gesture=false;return;}QTableView::mouseReleaseEvent(e);}
};
class Editor : public QMainWindow {
public:
    Document doc;
    std::deque<Document> undoStack,redoStack;
    FrameModel* model;
    QTableView* grid;
    QListWidget* library;
    QLineEdit *name,*search;
    QComboBox *clock,*rate,*preview;
    std::array<QCheckBox*,3> audible;
    QCheckBox* linked;
    Playback playback;
    std::array<std::vector<Cell>,3> clipboard;
    int clipboardChannel=0;
    QTimer recovery;
    QString recoveryFile;
    bool refreshing=false,testing=false;
    unsigned mask()const{unsigned result=0;for(int c=0;c<3;c++)if(audible[c]->isChecked())result|=1u<<c;return result;}
    EffectGroup& group(){return doc.project.groups[doc.group];}
    int channel()const{return std::max(0,grid->currentIndex().column())/8;}
    int track()const{auto& g=doc.project.groups[doc.group];int t=g.tracks[channel()];if(t>=0)return t;for(int v:g.tracks)if(v>=0)return v;return 0;}
    int row()const{return std::max(0,grid->currentIndex().row());}
    std::pair<int,int> selection()const{int a=row(),b=a;for(auto i:grid->selectionModel()->selectedIndexes()){a=std::min(a,i.row());b=std::max(b,i.row());}return {a,b};}
    void safe(std::function<void()> f){try{f();}catch(const std::exception& e){if(testing)throw;showError(this,e);}}
    void remember(){playback.stop();undoStack.push_back(doc);if(undoStack.size()>16)undoStack.pop_front();redoStack.clear();doc.dirty=true;}
    void history(bool redo){auto& src=redo?redoStack:undoStack;auto& dst=redo?undoStack:redoStack;if(src.empty())return;playback.stop();dst.push_back(doc);doc=std::move(src.back());src.pop_back();doc.dirty=true;refresh();}
    QAction* action(QMenu* menu,const QString& title,const QKeySequence& keys,std::function<void()> fn){
        auto a=menu->addAction(title);if(!keys.isEmpty())a->setShortcut(keys);connect(a,&QAction::triggered,this,[this,fn]{safe(fn);});return a;
    }
    void refresh(){
        refreshing=true;int oldRow=row(),oldColumn=std::max(0,grid->currentIndex().column());library->clear();for(int i=0;i<int(doc.project.groups.size());i++){auto& g=doc.project.groups[i];QString label=textString(g.name);int count=0;bool favorite=false;QString tags;for(int t:g.tracks)if(t>=0){count++;favorite|=doc.bank[t].favorite;tags+=" "+textString(doc.bank[t].tags);}
            auto item=new QListWidgetItem((favorite?"★ ":"")+label+QString("   [%1 CH]").arg(count),library);item->setData(Qt::UserRole,i);item->setHidden(!(label+tags).contains(search->text(),Qt::CaseInsensitive));}
        library->setCurrentRow(doc.group);name->setText(textString(group().name));clock->setCurrentIndex(clock->findData(doc.project.clock));rate->setCurrentIndex(rate->findData(doc.project.rate));
        model->reset();for(int c=0;c<3;c++)for(int f=0;f<8;f++)grid->setColumnHidden(c*8+f,group().tracks[c]<0);
        for(int c=0;c<3;c++)audible[c]->setEnabled(group().tracks[c]>=0);
        if(group().tracks[oldColumn/8]<0){int c=0;while(c<3&&group().tracks[c]<0)c++;oldColumn=c*8+2;}grid->setCurrentIndex(model->index(oldRow,oldColumn));
        setWindowTitle("AYFXEDIT NEXT GEN — "+(doc.path.empty()?QString("Untitled"):pathString(doc.path.filename()))+(doc.dirty?" *":""));
        int len=0;for(int t:group().tracks)if(t>=0)len=std::max(len,length(doc.bank[t]));
        statusBar()->showMessage(QString("%1 tracks · %2 frames · %3 s · %4 Hz%5").arg(doc.bank.size()).arg(len).arg(double(len)/doc.project.rate,0,'f',3).arg(doc.project.rate).arg(group().loopStart>=0?QString(" · Loop %1–%2").arg(group().loopStart).arg(group().loopEnd):""));
        refreshing=false;
    }
    bool save(bool as=false){
        auto path=doc.path;if(as||path.empty()){auto selected=QFileDialog::getSaveFileName(this,"Save bank",pathString(path),"AY bank (*.afb)");if(selected.isEmpty())return false;if(!selected.endsWith(".afb",Qt::CaseInsensitive))selected+=".afb";path=filePath(selected);}
        saveProjectBank(path,doc.bank,doc.project);doc.path=path;doc.dirty=false;QFile::remove(recoveryFile);refresh();return true;
    }
    bool discard(){if(!doc.dirty)return true;auto result=QMessageBox::question(this,"Unsaved changes","Save changes to the current bank?",QMessageBox::Save|QMessageBox::Discard|QMessageBox::Cancel);return result==QMessageBox::Discard||(result==QMessageBox::Save&&save());}
    void load(const fs::path& path,bool recovered=false){
        auto bytes=read(path);Document next;
        if(pathString(path.extension()).compare(".afx",Qt::CaseInsensitive)==0){next.bank={decode(bytes,0,bytes.size())};next.bank[0].name=path.stem().u8string();next.project={groupBank(next.bank),doc.project.clock,doc.project.rate};}
        else{next.bank=decodeBank(bytes);next.project={groupBank(next.bank),1789773,50};auto side=projectPath(path);if(fs::exists(side))next.project=decodeProject(read(side,256*1024*1024),next.bank,bytes);if(!recovered)next.path=path;}
        next.dirty=recovered;playback.stop();doc=std::move(next);undoStack.clear();redoStack.clear();refresh();
    }
    void play(bool solo=false,bool from=false){
        unsigned audibleMask=solo?1u<<channel():mask();std::array<const Effect*,3> channels{};int len=1,count=0;for(int c=0;c<3;c++)if(group().tracks[c]>=0){channels[c]=&doc.bank[group().tracks[c]];len=std::max(len,length(*channels[c]));count++;}
        int start=from?row():0;
        if(count==1&&group().loopStart<0&&!hasEnvelope(doc.bank[track()])&&preview->currentIndex()==1){playback.wav(ayfx::render(doc.bank[track()],doc.project.clock,doc.project.rate,start,PreviewModel::Legacy));return;}
        if(group().loopStart>=0)len=group().loopEnd;
        if(start>=len)start=0;
        auto stream=std::make_unique<AYStream>(registerFrames(channels,len),doc.project.clock,doc.project.rate,audibleMask,count==1?21845:10000,start,group().loopStart,group().loopEnd);
        playback.start(std::make_unique<AYDevice>(std::move(stream),group().loopStart>=0?-1:qint64(len-start+3)*SampleRate/doc.project.rate));
    }
    void editRows(bool insert,bool duplicate=false){
        auto [first,last]=selection();int count=last-first+1;auto g=group();int ch=channel();remember();
        for(int t:g.tracks)if(t>=0&&(linked->isChecked()||t==track())){
            auto& cells=doc.bank[t].cells;
            if(insert){for(int f=MaxFrames-1;f>=first+count;f--)cells[f]=cells[f-count];for(int f=0;f<count;f++)if(!duplicate)cells[first+f]={};}
            else{for(int f=first;f<MaxFrames-count;f++)cells[f]=cells[f+count];for(int f=MaxFrames-count;f<MaxFrames;f++)cells[f]={};}
        }
        if(linked->isChecked()&&group().loopStart>=0){
            auto adjust=[&](int pos){return insert?(pos>=first?std::min(MaxFrames,pos+count):pos):(pos<=first?pos:std::max(first,pos-count));};
            group().loopStart=adjust(g.loopStart);group().loopEnd=adjust(g.loopEnd);if(group().loopEnd<=group().loopStart)group().loopStart=group().loopEnd=-1;
        }refresh();grid->setCurrentIndex(model->index(first,ch*8+2));
    }
    void copyRows(){auto [first,last]=selection();clipboard={};clipboardChannel=channel();for(int c=0;c<3;c++){int t=group().tracks[c];if(t>=0&&(linked->isChecked()||c==channel()))clipboard[c].assign(doc.bank[t].cells.begin()+first,doc.bank[t].cells.begin()+last+1);}}
    void pasteRows(){
        int count=0;for(auto& c:clipboard)count=std::max(count,int(c.size()));if(!count)return;
        int first=row();remember();for(int c=0;c<3;c++){int t=group().tracks[c];if(t<0||(!linked->isChecked()&&c!=channel()))continue;auto& data=doc.bank[t].cells;
            for(int f=MaxFrames-1;f>=first+count;f--)data[f]=data[f-count];
            auto& source=clipboard[linked->isChecked()?c:clipboardChannel];for(int f=0;f<count&&first+f<MaxFrames;f++)data[first+f]=f<int(source.size())?source[f]:Cell{};
        }
        if(linked->isChecked()&&group().loopStart>=0){if(group().loopStart>=first)group().loopStart=std::min(MaxFrames,group().loopStart+count);if(group().loopEnd>=first)group().loopEnd=std::min(MaxFrames,group().loopEnd+count);if(group().loopEnd<=group().loopStart)group().loopStart=group().loopEnd=-1;}refresh();
    }
    void channelDialog(){
        QDialog d(this);d.setWindowTitle("Channels, loops and priority");auto form=new QFormLayout(&d);QLineEdit title(textString(group().name));form->addRow("Effect name",&title);
        std::array<QComboBox*,3> boxes;
        for(int c=0;c<3;c++){boxes[c]=new QComboBox;boxes[c]->addItem("Unused",-1);boxes[c]->addItem("New blank track",-2);for(int t=0;t<int(doc.bank.size());t++)boxes[c]->addItem(textString(doc.bank[t].name),t);boxes[c]->setCurrentIndex(boxes[c]->findData(group().tracks[c]));form->addRow(QString("Channel %1").arg(QChar('A'+c)),boxes[c]);}
        auto a=numberBox(group().loopStart,-1,MaxFrames-1),b=numberBox(group().loopEnd,-1,MaxFrames),priority=numberBox(group().priority,0,255);
        form->addRow("Loop start (-1 = off)",a);form->addRow("Loop end (exclusive)",b);form->addRow("Priority",priority);
        QDialogButtonBox buttons(QDialogButtonBox::Ok|QDialogButtonBox::Cancel);form->addRow(&buttons);connect(&buttons,&QDialogButtonBox::accepted,&d,&QDialog::accept);connect(&buttons,&QDialogButtonBox::rejected,&d,&QDialog::reject);
        if(d.exec()!=QDialog::Accepted)return;
        auto bank=doc.bank;auto groups=doc.project.groups;int index=assignChannels(bank,groups,doc.group,title.text().toUtf8().toStdString(),{{boxes[0]->currentData().toInt(),boxes[1]->currentData().toInt(),boxes[2]->currentData().toInt()}},a->value(),a->value()<0?-1:b->value());groups[index].priority=priority->value();
        remember();doc.bank=std::move(bank);doc.project.groups=std::move(groups);doc.group=index;refresh();
    }
    void generate(){
        QDialog d(this);d.setWindowTitle("Sound generator");auto form=new QFormLayout(&d);QComboBox preset;preset.addItems({"Laser","Explosion","Jump","Pickup","Footsteps","Engine"});auto frames=numberBox(24,1,MaxFrames),channels=numberBox(1,1,3);auto start=realBox(1200,20,20000),end=realBox(100,20,20000),noise=realBox(.5,0,1);
        form->addRow("Preset",&preset);form->addRow("Frames",frames);form->addRow("Channels",channels);form->addRow("Start Hz",start);form->addRow("End Hz",end);form->addRow("Noise",noise);
        QDialogButtonBox buttons(QDialogButtonBox::Ok|QDialogButtonBox::Cancel);form->addRow(&buttons);connect(&buttons,&QDialogButtonBox::accepted,&d,&QDialog::accept);connect(&buttons,&QDialogButtonBox::rejected,&d,&QDialog::reject);if(d.exec()!=QDialog::Accepted)return;
        auto result=generateSound({preset.currentIndex(),frames->value(),channels->value(),doc.project.clock,start->value(),end->value(),noise->value()});
        auto bank=doc.bank;auto groups=doc.project.groups;int g=applyConverted(bank,groups,doc.group,result,preset.currentText().toStdString(),false);remember();doc.bank=std::move(bank);doc.project.groups=std::move(groups);doc.group=g;refresh();
    }
    void workshop(bool reopen=false){
        WorkshopDialog d(doc.project.clock,doc.project.rate,this,reopen?&doc.bank[track()]:nullptr);
        if(d.exec()!=QDialog::Accepted)return;bool replace=QMessageBox::question(this,"Apply conversion","Replace the selected effect? Choose No to add a new effect.",QMessageBox::Yes|QMessageBox::No)==QMessageBox::Yes;
        auto bank=doc.bank;auto groups=doc.project.groups;int g=applyConverted(bank,groups,doc.group,d.output(),replace?group().name:"Converted audio",replace);
        for(int t:groups[g].tracks)if(t>=0){bank[t].sourcePath=d.sourcePath();bank[t].recipe=d.recipe();bank[t].sourceAudio=d.sourceExcerpt();break;}
        remember();doc.bank=std::move(bank);doc.project.groups=std::move(groups);doc.group=g;refresh();
    }
    void metadata(){
        QDialog d(this);d.setWindowTitle("Effect metadata");QFormLayout form(&d);QLineEdit tags(textString(doc.bank[track()].tags));QCheckBox favorite("Favourite");favorite.setChecked(doc.bank[track()].favorite);form.addRow("Tags",&tags);form.addRow(&favorite);
        QDialogButtonBox buttons(QDialogButtonBox::Ok|QDialogButtonBox::Cancel);form.addRow(&buttons);connect(&buttons,&QDialogButtonBox::accepted,&d,&QDialog::accept);connect(&buttons,&QDialogButtonBox::rejected,&d,&QDialog::reject);
        if(d.exec()==QDialog::Accepted){remember();for(int t:group().tracks)if(t>=0){doc.bank[t].tags=tags.text().toUtf8().toStdString();doc.bank[t].favorite=favorite.isChecked();}refresh();}
    }
    void envelope(){
        QDialog d(this);d.setWindowTitle("Envelope on selected frames");QFormLayout form(&d);QCheckBox enabled("Use hardware envelope");enabled.setChecked(true);auto period=numberBox(100,0,65535),shape=numberBox(10,-1,15);form.addRow(&enabled);form.addRow("Period",period);form.addRow("Trigger at first frame (-1 = none)",shape);QDialogButtonBox buttons(QDialogButtonBox::Ok|QDialogButtonBox::Cancel);form.addRow(&buttons);
        connect(&buttons,&QDialogButtonBox::accepted,&d,&QDialog::accept);connect(&buttons,&QDialogButtonBox::rejected,&d,&QDialog::reject);if(d.exec()!=QDialog::Accepted)return;
        auto [a,b]=selection();remember();for(int f=a;f<=b;f++){auto& c=doc.bank[track()].cells[f];c.envelope=enabled.isChecked();c.envelopePeriod=period->value();c.shape=f==a?shape->value():-1;}refresh();
    }
    void curve(){
        QDialog d(this);d.setWindowTitle("Curve on selected frames");QFormLayout form(&d);QComboBox lane;lane.addItems({"Volume","Tone period","Noise period"});auto a=realBox(1,0,1),b=realBox(0,0,1);QCheckBox smooth("Smooth interpolation");form.addRow("Parameter",&lane);form.addRow("Start (0–1)",a);form.addRow("End (0–1)",b);form.addRow(&smooth);QDialogButtonBox buttons(QDialogButtonBox::Ok|QDialogButtonBox::Cancel);form.addRow(&buttons);
        connect(&buttons,&QDialogButtonBox::accepted,&d,&QDialog::accept);connect(&buttons,&QDialogButtonBox::rejected,&d,&QDialog::reject);if(d.exec()!=QDialog::Accepted)return;
        auto [first,last]=selection();remember();applyCurve(doc.bank[track()],first,last,lane.currentIndex(),{{0,a->value()},{1,b->value()}},smooth.isChecked());refresh();
    }
    void capture(){
        auto p=QFileDialog::getOpenFileName(this,"Extract capture",{},"AY captures (*.psg *.vgm)");if(p.isEmpty())return;auto raw=read(filePath(p));auto cap=p.endsWith(".vgm",Qt::CaseInsensitive)?captureVGM(raw,doc.project.rate):capturePSG(raw,doc.project.rate);if(!p.endsWith(".vgm",Qt::CaseInsensitive))cap.clock=doc.project.clock;
        bool ok;int start=QInputDialog::getInt(this,"Capture range","First frame",0,0,std::max(0,int(cap.frames.size())-1),1,&ok);if(!ok)return;
        int count=QInputDialog::getInt(this,"Capture range","Number of frames",std::min(MaxFrames,int(cap.frames.size())-start),1,std::min(MaxFrames,int(cap.frames.size())-start),1,&ok);if(!ok)return;
        auto result=extractCapture(cap,start,start+count,7);auto bank=doc.bank;auto groups=doc.project.groups;int g=applyConverted(bank,groups,doc.group,result,QFileInfo(p).baseName().toUtf8().toStdString(),false);remember();doc.bank=std::move(bank);doc.project.groups=std::move(groups);doc.group=g;doc.project.clock=cap.clock;refresh();
    }
    void exportFile(int kind){
        const char* filters[]={"WAV (*.wav)","CSV (*.csv)","VT2 sample (*.vt2)","AY effect (*.afx)","AY registers (*.ayregs *.psg11)"};
        const char* extensions[]={".wav",".csv",".vt2",".afx",".ayregs"};
        auto p=QFileDialog::getSaveFileName(this,"Export",textString(group().name)+extensions[kind],filters[kind]);if(p.isEmpty())return;
        if(QFileInfo(p).suffix().isEmpty())p+=extensions[kind];
        if(kind==4){exportRegisters(filePath(p),doc.bank,group(),doc.project.clock,doc.project.rate);return;}
        Bytes bytes;auto& e=doc.bank[track()];
        if(kind==0){std::array<const Effect*,3> channels{};int count=0;for(int c=0;c<3;c++)if(group().tracks[c]>=0){channels[c]=&doc.bank[group().tracks[c]];count++;}
            if(count==1&&preview->currentIndex()==1&&group().loopStart<0&&!hasEnvelope(e))bytes=ayfx::render(e,doc.project.clock,doc.project.rate,0,PreviewModel::Legacy);
            else bytes=renderChannels(channels,doc.project.clock,doc.project.rate,0,count==1?21845:10000,mask(),group().loopStart>=0?group().loopEnd:-1,0);
        }else{requireClassic(e);bytes=kind==1?csv(e):kind==2?vt2(e,doc.project.clock):encode(e);}write(filePath(p),bytes);
    }
    void importEffects(){
        auto files=QFileDialog::getOpenFileNames(this,"Import effects",{},"AY effects (*.afx)");if(files.isEmpty())return;
        auto next=doc.bank;for(auto p:files){require(next.size()<MaxEffects,"Bank is full");auto bytes=read(filePath(p));auto effect=decode(bytes,0,bytes.size());effect.name=QFileInfo(p).baseName().toUtf8().toStdString();next.push_back(std::move(effect));}
        remember();int first=int(doc.bank.size());doc.bank=std::move(next);for(int t=first;t<int(doc.bank.size());t++)doc.project.groups.push_back({doc.bank[t].name,{{t,-1,-1}}});doc.group=int(doc.project.groups.size())-1;refresh();
    }
    void help(){
        auto d=new QDialog(this);d->setAttribute(Qt::WA_DeleteOnClose);d->setWindowTitle("AYFXEDIT NEXT GEN — offline guide");d->resize(960,680);auto layout=new QHBoxLayout(d);auto tree=new QTreeWidget;tree->setHeaderHidden(true);auto body=new QTextBrowser;body->setOpenExternalLinks(false);layout->addWidget(tree,1);layout->addWidget(body,3);
        QMap<QString,QTreeWidgetItem*> sections;
        for(size_t i=0;i<helpTopicCount;i++){QString section=QString::fromWCharArray(helpTopics[i].section);if(!sections.contains(section))sections[section]=new QTreeWidgetItem(tree,{section});auto item=new QTreeWidgetItem(sections[section],{QString::fromWCharArray(helpTopics[i].title)});item->setData(0,Qt::UserRole,int(i));}
        connect(tree,&QTreeWidget::currentItemChanged,d,[body](QTreeWidgetItem* item,QTreeWidgetItem*){if(!item||!item->parent())return;auto& topic=helpTopics[item->data(0,Qt::UserRole).toInt()];body->setPlainText("Native preview: menu placement and keyboard shortcuts follow your operating system. This port uses Qt for audio output and MP3 decoding. Some Windows-only UI instructions differ. See NATIVE_PORT.md supplied with this build.\n\n"+QString::fromWCharArray(topic.title)+"\n\n"+QString::fromWCharArray(topic.body));});
        tree->expandAll();tree->setCurrentItem(tree->topLevelItem(0)->child(0));d->show();
    }
    void autosave(){if(!doc.dirty||testing)return;try{QDir().mkpath(QFileInfo(recoveryFile).absolutePath());saveProjectBank(filePath(recoveryFile),doc.bank,doc.project);}catch(const std::exception& e){statusBar()->showMessage("Recovery snapshot failed: "+QString::fromUtf8(e.what()));}}
    void recover(){if(!QFileInfo::exists(recoveryFile)){QMessageBox::information(this,"Recovery","No recovery snapshot is available.");return;}if(discard())load(filePath(recoveryFile),true);}
    void dragEnterEvent(QDragEnterEvent* event) override {
        auto urls=event->mimeData()->urls();if(urls.size()==1&&urls[0].isLocalFile()){
            auto ext=QFileInfo(urls[0].toLocalFile()).suffix().toLower();if(ext=="afx"||ext=="afb")event->acceptProposedAction();
        }
    }
    void dropEvent(QDropEvent* event) override {
        auto urls=event->mimeData()->urls();if(urls.size()!=1||!urls[0].isLocalFile())return;
        safe([&]{if(discard()){load(filePath(urls[0].toLocalFile()));event->acceptProposedAction();}});
    }
    void batchExport() {
        for(auto& effect:doc.bank)requireClassic(effect);
        auto selected=QFileDialog::getSaveFileName(this,"Choose a NEW folder for classic AFX export","ayfx-classic");
        if(selected.isEmpty())return;auto folder=filePath(selected);require(!fs::exists(folder),"Choose a new folder; existing files are not overwritten");
        fs::create_directories(folder);
        for(size_t i=0;i<doc.bank.size();i++)write(folder/(std::to_string(i)+".afx"),encode(doc.bank[i]));
        statusBar()->showMessage(QString("Exported %1 classic tracks. Group metadata stays in the project.").arg(doc.bank.size()));
    }
    void closeEvent(QCloseEvent* event) override {try{if(discard()){playback.stop();QFile::remove(recoveryFile);QFile::remove(recoveryFile+".ayfx-project");event->accept();}else event->ignore();}catch(const std::exception& e){showError(this,e);event->ignore();}}
    explicit Editor(bool test=false):testing(test){
        setAcceptDrops(true);setWindowTitle("AYFXEDIT NEXT GEN");resize(1560,860);
        auto central=new QWidget;setCentralWidget(central);auto layout=new QVBoxLayout(central);auto top=new QHBoxLayout;layout->addLayout(top);
        auto addButton=[&](const QString& label,std::function<void()> fn){auto b=new QPushButton(label);top->addWidget(b);connect(b,&QPushButton::clicked,this,[this,fn]{safe(fn);});};
        addButton("Open",[this]{if(!discard())return;auto p=QFileDialog::getOpenFileName(this,"Open",{},"AY banks/effects (*.afb *.afx)");if(!p.isEmpty())load(filePath(p));});addButton("Save",[this]{save();});addButton("Play all",[this]{play();});addButton("Solo",[this]{play(true);});addButton("Stop",[this]{playback.stop();});addButton("Channels / loop…",[this]{channelDialog();});
        for(int c=0;c<3;c++){audible[c]=new QCheckBox(QString("%1 sound").arg(QChar('A'+c)));audible[c]->setChecked(true);top->addWidget(audible[c]);connect(audible[c],&QCheckBox::toggled,this,[this](bool){playback.stop();});}
        auto options=new QHBoxLayout;layout->addLayout(options);name=new QLineEdit;options->addWidget(name,2);
        clock=new QComboBox;clock->addItem("MSX · 1.789773 MHz",1789773);clock->addItem("Spectrum · 1.7734 MHz",1773400);clock->addItem("2 MHz",2000000);rate=new QComboBox;rate->addItem("50 Hz",50);rate->addItem("60 Hz",60);preview=new QComboBox;preview->addItems({"Reference AY","Legacy (single channel)"});linked=new QCheckBox("Link frame edits");linked->setChecked(true);
        options->addWidget(clock);options->addWidget(rate);options->addWidget(preview);options->addWidget(linked);
        auto splitter=new QSplitter;layout->addWidget(splitter,1);auto sidebar=new QWidget;auto side=new QVBoxLayout(sidebar);side->setContentsMargins(0,0,0,0);search=new QLineEdit;search->setPlaceholderText("Search names and tags");library=new QListWidget;side->addWidget(search);side->addWidget(library);splitter->addWidget(sidebar);
        grid=new FrameView;grid->setItemDelegate(new ParameterDelegate(grid));grid->setToolTip("Edit hexadecimal values or Alt-drag tone, noise and volume cells to draw parameter bars.");model=new FrameModel(&doc,this);grid->setModel(model);grid->setSelectionMode(QAbstractItemView::ExtendedSelection);grid->setFont(QFontDatabase::systemFont(QFontDatabase::FixedFont));grid->verticalHeader()->setDefaultSectionSize(25);grid->horizontalHeader()->setSectionResizeMode(QHeaderView::Interactive);int widths[]={36,36,58,46,48,36,64,46};for(int i=0;i<24;i++)grid->setColumnWidth(i,widths[i%8]);splitter->addWidget(grid);splitter->setStretchFactor(1,1);splitter->setSizes({250,1000});
        model->beforeEdit=[this]{remember();};model->afterEdit=[this]{if(!windowTitle().endsWith(" *"))setWindowTitle(windowTitle()+" *");statusBar()->showMessage("Edited — Ctrl/Cmd+S to save");};
        connect(search,&QLineEdit::textChanged,this,[this]{if(!refreshing)refresh();});
        connect(library,&QListWidget::currentRowChanged,this,[this](int i){if(refreshing||i<0)return;playback.stop();doc.group=i;refresh();});
        connect(name,&QLineEdit::editingFinished,this,[this]{if(refreshing)return;safe([this]{auto value=name->text().toUtf8().toStdString();require(!value.empty()&&value.size()<=255,"Name must contain 1–255 UTF-8 bytes");if(value!=group().name){remember();group().name=value;refresh();}});});
        connect(clock,QOverload<int>::of(&QComboBox::currentIndexChanged),this,[this](int){if(!refreshing){remember();doc.project.clock=clock->currentData().toInt();refresh();}});
        connect(rate,QOverload<int>::of(&QComboBox::currentIndexChanged),this,[this](int){if(!refreshing){remember();doc.project.rate=rate->currentData().toInt();refresh();}});
        auto file=menuBar()->addMenu("&File");action(file,"New",QKeySequence::New,[this]{if(discard()){doc=Document{};undoStack.clear();redoStack.clear();refresh();}});
        action(file,"Open…",QKeySequence::Open,[this]{if(!discard())return;auto p=QFileDialog::getOpenFileName(this,"Open bank",{},"AY banks/effects (*.afb *.afx)");if(!p.isEmpty())load(filePath(p));});
        action(file,"Save",QKeySequence::Save,[this]{save();});action(file,"Save as…",QKeySequence::SaveAs,[this]{save(true);});
        action(file,"Import AFX files…",{},[this]{importEffects();});action(file,"Batch export classic AFX…",{},[this]{batchExport();});action(file,"WAV / MP3 workshop…",{},[this]{workshop();});
        for(int kind=0;kind<5;kind++)action(file,QString("Export %1…").arg(QStringList{"WAV","CSV","VT2","AFX (active track)","AY registers"}[kind]),{},[this,kind]{exportFile(kind);});
        action(file,"Export game SDK…",{},[this]{auto p=QFileDialog::getSaveFileName(this,"Choose a NEW export folder name","ayfx-sdk");if(!p.isEmpty()){auto result=exportBankData(filePath(p),doc.bank,doc.project);statusBar()->showMessage(QString("Exported %1 effects, %2 bytes").arg(result.effects).arg(result.bytes));}});
        action(file,"Quit",QKeySequence::Quit,[this]{close();});
        auto edit=menuBar()->addMenu("&Edit");action(edit,"Undo",QKeySequence::Undo,[this]{history(false);});action(edit,"Redo",QKeySequence::Redo,[this]{history(true);});action(edit,"Copy frames",QKeySequence::Copy,[this]{copyRows();});action(edit,"Cut frames",QKeySequence::Cut,[this]{copyRows();editRows(false);});action(edit,"Paste frames",QKeySequence::Paste,[this]{pasteRows();});
        action(edit,"Insert frames",QKeySequence(Qt::Key_Insert),[this]{editRows(true);});action(edit,"Duplicate frames",QKeySequence(Qt::CTRL|Qt::Key_Insert),[this]{editRows(true,true);});action(edit,"Delete frames",QKeySequence(Qt::Key_Delete),[this]{editRows(false);});
        action(edit,"Add effect",{},[this]{require(doc.bank.size()<MaxEffects,"Bank is full");remember();int t=int(doc.bank.size());doc.bank.emplace_back();doc.project.groups.push_back({"Untitled",{{t,-1,-1}}});doc.group=int(doc.project.groups.size())-1;refresh();});
        action(edit,"Duplicate effect",{},[this]{int count=0;for(int t:group().tracks)if(t>=0)count++;require(doc.bank.size()+count<=MaxEffects,"Bank is full");auto g=group();remember();for(auto& t:g.tracks)if(t>=0){auto effect=doc.bank[t];t=int(doc.bank.size());doc.bank.push_back(std::move(effect));}g.name+=" copy";doc.project.groups.push_back(g);doc.group=int(doc.project.groups.size())-1;refresh();});
        action(edit,"Remove active channel",{},[this]{require(doc.bank.size()>1,"Cannot remove the final track");remember();removeTrack(doc.bank,doc.project.groups,track());doc.group=std::min(doc.group,int(doc.project.groups.size())-1);refresh();});
        auto sound=menuBar()->addMenu("&Sound");action(sound,"Play",QKeySequence(Qt::Key_Return),[this]{play();});action(sound,"Play from cursor",QKeySequence(Qt::CTRL|Qt::Key_Return),[this]{play(false,true);});action(sound,"Stop",QKeySequence(Qt::Key_Space),[this]{playback.stop();});action(sound,"Channels / loops…",{},[this]{channelDialog();});
        action(sound,"Generator…",{},[this]{generate();});action(sound,"Curve…",{},[this]{curve();});action(sound,"Hardware envelope…",{},[this]{envelope();});
        action(sound,"Transpose…",{},[this]{bool ok;double pitch=QInputDialog::getDouble(this,"Transpose","Semitones",0,-24,24,2,&ok);if(!ok)return;auto [a,b]=selection();remember();for(int f=a;f<=b;f++){auto& c=doc.bank[track()].cells[f];c.tone=std::clamp(int(std::lround(c.tone/std::exp2(pitch/12))),1,4095);}refresh();});
        action(sound,"Vibrato…",{},[this]{bool ok;double depth=QInputDialog::getDouble(this,"Vibrato","Depth in semitones",1,0,24,2,&ok);if(!ok)return;double cycles=QInputDialog::getDouble(this,"Vibrato","Cycles over selected frames",2,0,64,2,&ok);if(!ok)return;auto [a,b]=selection();remember();modulate(doc.bank[track()],a,b,depth,cycles);refresh();});
        auto tools=menuBar()->addMenu("&Tools");action(tools,"Metadata / favourites…",{},[this]{metadata();});action(tools,"Extract PSG / VGM…",{},[this]{capture();});action(tools,"Reopen conversion source…",{},[this]{require(!doc.bank[track()].sourceAudio.empty(),"This track has no saved source audio");workshop(true);});action(tools,"Recover autosave…",{},[this]{recover();});
        auto helpMenu=menuBar()->addMenu("&Help");action(helpMenu,"User guide",QKeySequence(Qt::Key_F1),[this]{help();});action(helpMenu,"About",{},[this]{QMessageBox::about(this,"AYFXEDIT NEXT GEN","Native desktop preview\n\nOriginal: Shiru\nImproved fork: Threetwosevensixseven\nNEXT GEN: Max Iwamoto\n\nQt provides the native interface, audio output and decoding.\nSee CREDITS.md and THIRD_PARTY_NOTICES.md.");});
        auto piano=new QHBoxLayout;layout->addLayout(piano);auto octave=numberBox(4,0,8);piano->addWidget(new QLabel("Octave"));piano->addWidget(octave);
        QStringList notes{"C","C#","D","D#","E","F","F#","G","G#","A","A#","B"};for(int n=0;n<12;n++){auto key=new QPushButton(notes[n]);piano->addWidget(key);connect(key,&QPushButton::clicked,this,[this,octave,n]{remember();auto& cell=doc.bank[track()].cells[row()];double hz=440*std::exp2((octave->value()*12+n-57)/12.);cell.tone=std::clamp(int(doc.project.clock/(16*hz)),1,4095);cell.volume=15;cell.t=true;refresh();});}
        recoveryFile=QStandardPaths::writableLocation(QStandardPaths::AppLocalDataLocation)+"/Recovery/session.afb";connect(&recovery,&QTimer::timeout,this,[this]{autosave();});if(!testing)recovery.start(60000);refresh();
        if(!testing&&QFileInfo::exists(recoveryFile))QTimer::singleShot(0,this,[this]{if(QMessageBox::question(this,"Recovery available","Open the recovery snapshot from the previous session?")==QMessageBox::Yes)safe([this]{recover();});});
    }
    void selfTest(const fs::path& folder){
        fs::create_directories(folder);
        auto made=generateSound({0,24,3});doc.bank=made.tracks;doc.project={{{"Native test",{{0,1,2}},4,20}},1789773,60};refresh();
        require(model->setData(model->index(2,2),"ABC",Qt::EditRole),"Grid edit failed");require(doc.bank[0].cells[2].tone==0xabc,"Hex editing changed value");history(false);require(doc.bank[0].cells[2].tone!=0xabc,"Undo failed");history(true);require(doc.bank[0].cells[2].tone==0xabc,"Redo failed");
        doc.bank[0].name=u8"音声";doc.project.groups[0].name=u8"Native café 音声";doc.path=folder/fs::u8path(u8"音声.afb");saveProjectBank(doc.path,doc.bank,doc.project);auto expected=encodeRegisters(doc.bank,group(),1789773,60,false);
        auto path=doc.path;load(path);auto actual=encodeRegisters(doc.bank,group(),1789773,60,false);require(actual.data==expected.data,"Project save/load changed registers");require(group().name==u8"Native café 音声","Unicode name lost");
        auto before=doc.bank[0].cells[4].tone;grid->setCurrentIndex(model->index(4,2));grid->clearSelection();editRows(true);require(group().loopStart==5&&group().loopEnd==21,"Linked insert lost loop timing");require(doc.bank[0].cells[5].tone==before,"Linked insert lost frame");history(false);
        auto pcm=renderChannels({&doc.bank[0],&doc.bank[1],&doc.bank[2]},doc.project.clock,doc.project.rate);write(folder/"preview.wav",pcm);exportRegisters(folder/"test.ayregs",doc.bank,group(),doc.project.clock,doc.project.rate);
        auto sdk=folder/"sdk";if(!fs::exists(sdk))exportBankData(sdk,doc.bank,doc.project);
        Effect saved;std::vector<float> samples(SampleRate/2);for(size_t i=0;i<samples.size();i++)samples[i]=float(.4*std::sin(2*Pi*440*i/SampleRate));saved.sourceAudio=pcmWave(samples);saved.sourcePath="synthetic-tone.wav";
        {WorkshopDialog workshop(doc.project.clock,doc.project.rate,this,&saved);workshop.setAttribute(Qt::WA_DontShowOnScreen);workshop.show();QApplication::processEvents();workshop.grab().save(pathString(folder/"workshop.png"));workshop.reject();}
        doc.dirty=false;setAttribute(Qt::WA_DontShowOnScreen);show();QApplication::processEvents();grab().save(pathString(folder/"editor.png"));std::cout<<"PASS: native UI edits, undo/redo, Unicode project save/load, linked loops, WAV and register exports.\n";
    }
};
int main(int argc,char** argv){
    QApplication app(argc,argv);QCoreApplication::setOrganizationName("Max Iwamoto");QCoreApplication::setApplicationName("AYFXEDIT NEXT GEN");QApplication::setStyle("Fusion");
    try{
        if(argc>2&&(QString::fromLocal8Bit(argv[1])=="--decode-test"||QString::fromLocal8Bit(argv[1])=="--decode-reject")){
            bool reject=QString::fromLocal8Bit(argv[1])=="--decode-reject";int result=1;
            new AudioImport(QString::fromLocal8Bit(argv[2]),[&](SourceWave wave,QString error){
                try{if(reject){require(!error.isEmpty(),"Malformed MP3 was accepted");}
                    else{require(error.isEmpty(),error.toUtf8().constData());require(wave.duration()>.4&&wave.duration()<.7,"Decoded MP3 duration changed");double energy=0;for(auto v:wave.mono)energy+=v*v;require(energy>1,"Decoded MP3 is silent");
                        ConvertOptions options;options.start=.1;options.end=.3;options.channels=1;options.rate=60;auto converted=convertWave(wave,options);require(converted.frames==12,"Decoded MP3 conversion timing changed");int period=converted.tracks[0].cells[4].tone;require(period>230&&period<280,"Decoded MP3 conversion pitch changed");}
                    result=0;std::cout<<"PASS: Qt MP3 "<<(reject?"rejection":"decode, pitch and timing")<<"\n";
                }catch(const std::exception& e){std::cerr<<e.what()<<"\n";}
                app.exit(result);
            },&app);return app.exec();
        }
        bool test=argc>1&&QString::fromLocal8Bit(argv[1])=="--self-test";Editor editor(test);if(test){require(argc>2,"Self-test output folder required");editor.selfTest(filePath(QString::fromLocal8Bit(argv[2])));return 0;}
        if(argc>1)editor.load(filePath(QString::fromLocal8Bit(argv[1])));editor.show();return app.exec();
    }catch(const std::exception& e){std::cerr<<e.what()<<"\n";return 1;}
}
