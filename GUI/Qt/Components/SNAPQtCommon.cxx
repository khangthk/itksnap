#include "SNAPQtCommon.h"
#include <QPainter>
#include <QPixmap>
#include <QWidget>
#include <QAction>
#include <QObject>
#include <QPushButton>
#include <QMainWindow>
#include <QMessageBox>
#include <QApplication>
#include <QMenu>
#include <QComboBox>
#include <QStandardItemModel>
#include <QFileInfo>
#include <QGraphicsItem>
#include <QGraphicsScene>
#include <QGraphicsDropShadowEffect>


#include "QtCursorOverride.h"
#include "GlobalUIModel.h"
#include "SystemInterface.h"
#include "IRISApplication.h"
#include "HistoryManager.h"
#include "SimpleFileDialogWithHistory.h"
#include "ColorLabelTable.h"
#include "ColorMap.h"
#include "ColorMapModel.h"
#include "ImageIOWizard.h"


QIcon CreateColorBoxIcon(int w, int h, const QBrush &brush)
{
  QRect r(2,2,w-5,w-5);
  QPixmap pix(w, h);
  pix.fill(QColor(0,0,0,0));
  QPainter paint(&pix);
  paint.setPen(Qt::black);
  paint.setBrush(brush);
  paint.drawRect(r);
  return QIcon(pix);
}

QIcon CreateColorBoxIcon(int w, int h, const QColor &rgb)
{
  return CreateColorBoxIcon(w, h, QBrush(rgb));
}

QIcon CreateColorBoxIcon(int w, int h, const Vector3ui &rgb)
{
 return CreateColorBoxIcon(w, h, QColor(rgb(0), rgb(1), rgb(2)));
}

QIcon CreateInvisibleIcon(int w, int h)
{
  // Add initial entries to background
  QPixmap pix(w, h);
  pix.fill(QColor(0,0,0,0));
  return QIcon(pix);
}


#include <map>
#include <itkObject.h>

QIcon CreateColorMapIcon(int w, int h, ColorMap *cmap)
{
  // Maintain a static map of icons for each existing color map
  typedef std::pair<itk::TimeStamp, QIcon> StampedIcon;
  typedef std::map<ColorMap *, StampedIcon> IconMap;
  static IconMap icon_map;

  // Get the color map's timestamp
  itk::TimeStamp ts_cmap = cmap->GetTimeStamp();

  // Try to find the icon in the icon map
  IconMap::iterator it = icon_map.find(cmap);
  if(it != icon_map.end())
    {
    // We have created an icon for this before. Check that it's current and
    // that it matches the requested size (only one size is cached!)
    itk::TimeStamp ts_icon = it->second.first;
    if(ts_cmap == ts_icon)
      return it->second.second;
    }

  // Create the actual icon
  QPixmap pix(w, h);
  pix.fill(QColor(0,0,0,0));

  QPainter paint(&pix);
  for(int x = 3; x <= w-4; x++)
    {
    double t = (x - 3.0) / (w - 7.0);
    ColorMap::RGBAType rgba = cmap->MapIndexToRGBA(t);
    paint.setPen(QColor(rgba[0], rgba[1], rgba[2]));
    paint.drawLine(x, 3, x, w-4);
    }

  paint.setPen(Qt::black);
  QRect r(2,2,w-5,w-5);
  paint.drawRect(r);

  QIcon icon(pix);

  // Save the icon
  icon_map[cmap] = std::make_pair(ts_cmap, icon);
  return icon;
}

QStandardItem *CreateColorMapPresetItem(
    ColorMapModel *cmm, const std::string &preset)
{
  ColorMap *cm = cmm->GetPresetManager()->GetPreset(preset);
  QIcon icon = CreateColorMapIcon(16, 16, cm);

  QStandardItem *item = new QStandardItem(icon, from_utf8(preset));
  item->setData(QVariant::fromValue(preset), Qt::UserRole);

  return item;
}

void
PopulateColorMapPresetCombo(QComboBox *combo, ColorMapModel *model)
{
  // Get the list of system presets and custom presets from the model
  ColorMapModel::PresetList pSystem, pUser;
  model->GetPresets(pSystem, pUser);

  // What is the current item
  QVariant currentItemData = combo->itemData(combo->currentIndex(), Qt::UserRole);
  std::string currentPreset = currentItemData.value<std::string>();
  int newIndex = -1;

  // The system presets don't change, so we only need to set them the
  // first time around
  QStandardItemModel *sim = new QStandardItemModel();

  for(unsigned int i = 0; i < pSystem.size(); i++)
    {
    sim->appendRow(CreateColorMapPresetItem(model, pSystem[i]));
    if(currentPreset == pSystem[i])
      newIndex = i;
    }

  for(unsigned int i = 0; i < pUser.size(); i++)
    {
    sim->appendRow(CreateColorMapPresetItem(model, pUser[i]));
    if(currentPreset == pUser[i])
      newIndex = pSystem.size() + i;
    }

  // Update the model
  combo->setModel(sim);

  // Set the current item if possible
  combo->setCurrentIndex(newIndex);

  // Insert separator
  combo->insertSeparator(pSystem.size());
}



QBrush GetBrushForColorLabel(const ColorLabel &cl)
{
  return QBrush(QColor(cl.GetRGB(0), cl.GetRGB(1), cl.GetRGB(2)));
}

QBrush GetBrushForDrawOverFilter(DrawOverFilter flt, const ColorLabel &cl)
{
  switch(flt.CoverageMode)
    {
    case PAINT_OVER_VISIBLE:
      return QBrush(Qt::black, Qt::Dense6Pattern);
    case PAINT_OVER_ONE:
      return QBrush(QColor(cl.GetRGB(0), cl.GetRGB(1), cl.GetRGB(2)));
    case PAINT_OVER_ALL:
      return QBrush(Qt::black, Qt::BDiagPattern);
    }
  return QBrush();
}

QString GetTitleForColorLabel(const ColorLabel &cl)
{
  return QString::fromUtf8(cl.GetLabel());
}

QString GetTitleForDrawOverFilter(DrawOverFilter flt, const ColorLabel &cl)
{
  switch(flt.CoverageMode)
    {
    case PAINT_OVER_VISIBLE:
      return QString("All visible labels");
    case PAINT_OVER_ONE:
      return QString::fromUtf8(cl.GetLabel());
    case PAINT_OVER_ALL:
      return QString("All labels");
    }
  return QString();
}

QBrush GetBrushForColorLabel(int label, ColorLabelTable *clt)
{
  return GetBrushForColorLabel(clt->GetColorLabel(label));
}

QBrush GetBrushForDrawOverFilter(DrawOverFilter flt, ColorLabelTable *clt)
{
  return GetBrushForDrawOverFilter(flt, clt->GetColorLabel(flt.DrawOverLabel));
}

QString GetTitleForColorLabel(int label, ColorLabelTable *clt)
{
  return GetTitleForColorLabel(clt->GetColorLabel(label));
}

QString GetTitleForDrawOverFilter(DrawOverFilter flt, ColorLabelTable *clt)
{
  return GetTitleForDrawOverFilter(flt, clt->GetColorLabel(flt.DrawOverLabel));
}


QIcon CreateLabelComboIcon(int w, int h, LabelType fg, DrawOverFilter bg, ColorLabelTable *clt)
{
  // TODO: this could be made a little prettier
  QGraphicsScene scene(0,0,w,h);

  QPixmap pm(w, h);
  pm.fill(QColor(0,0,0,0));

  QPainter qp(&pm);

  QBrush brush_fg = GetBrushForColorLabel(fg, clt);
  QBrush brush_bg = GetBrushForDrawOverFilter(bg, clt);

  QGraphicsItem *item_bg = scene.addRect(w/3,h/3,w/2+1,h/2+1,QPen(Qt::black), brush_bg);
  scene.addRect(2,2,w/2+1,h/2+1,QPen(Qt::black), brush_fg);

  QGraphicsDropShadowEffect *eff_bg = new QGraphicsDropShadowEffect(&scene);
  eff_bg->setBlurRadius(1.0);
  eff_bg->setOffset(1.0);
  eff_bg->setColor(QColor(63,63,63,100));
  item_bg->setGraphicsEffect(eff_bg);

  scene.render(&qp);

  return QIcon(pm);
}

QString CreateLabelComboTooltip(LabelType fg, DrawOverFilter bg, ColorLabelTable *clt)
{
  return QString(
        "<html><head/><body>"
        "<p>Foreground label:<br><span style=\" font-weight:600;\">%1</span></p>"
        "<p>Background label:<br><span style=\" font-weight:600;\">%2</span></p>"
        "</body></html>").
      arg(GetTitleForColorLabel(fg, clt)).arg(GetTitleForDrawOverFilter(bg, clt));
}






QAction *FindUpstreamAction(QWidget *widget, const QString &targetActionName)
{
  // Look for a parent of QMainWindow type
  QMainWindow *topwin = NULL;
  for(QObject *p = widget; p != NULL; p = p->parent())
    {
    if((topwin = dynamic_cast<QMainWindow *>(p)) != NULL)
      break;
    }

  // If nothing found, try a global search
  if(!topwin)
    {
    QWidgetList lst = QApplication::topLevelWidgets();
    for(QWidgetList::Iterator it = lst.begin();
        it != lst.end(); ++it)
      {
      QWidget *w = *it;
      if((topwin = dynamic_cast<QMainWindow *>(w)) != NULL)
        break;
      }
    }

  // Look for the action
  QAction *result = NULL;
  if(topwin)
    {
    result = topwin->findChild<QAction *>(targetActionName);
    }

  if(!result)
      std::cerr << "Failed find upstream action " << targetActionName.toStdString() << std::endl;

  return result;
}

void ConnectWidgetToTopLevelAction(
    QWidget *w, const char *signal, QString actionName)
{
  QAction *action = FindUpstreamAction(w, actionName);
  QObject::connect(w, signal, action, SLOT(trigger()));
}

bool TriggerUpstreamAction(QWidget *widget, const QString &targetActionName)
{
  // Find and execute the relevant action
  QAction *action = FindUpstreamAction(widget, targetActionName);
  if(action)
    {
    action->trigger();
    return true;
    }
  else
    return false;
}

QStringList toQStringList(const std::vector<std::string> inlist)
{
  QStringList qsl;
  qsl.reserve(inlist.size());
  for(std::vector<std::string>::const_iterator it = inlist.begin();
      it != inlist.end(); ++it)
    {
    qsl.push_back(from_utf8(*it));
    }
  return qsl;
}

void ReportNonLethalException(QWidget *parent,
                              std::exception &exc,
                              QString windowTitleText,
                              QString mainErrorText)
{
  QMessageBox b(parent);
  b.setText(mainErrorText);
  b.setWindowTitle(QString("%1 - ITK-SNAP").arg(windowTitleText));
  b.setDetailedText(exc.what());
  b.setIcon(QMessageBox::Critical);
  b.exec();
}

void PopulateHistoryMenu(
    QMenu *menu, QObject *receiver, const char *slot,
    const QStringList &local_history,
    const QStringList &global_history)
{
  menu->clear();

  QStringListIterator itLocal(local_history);
  itLocal.toBack();
  while(itLocal.hasPrevious())
    menu->addAction(itLocal.previous(), receiver, slot);

  int nLocal = menu->actions().size();

  QStringListIterator itGlobal(global_history);
  itGlobal.toBack();
  while(itGlobal.hasPrevious())
    {
    QString entry = itGlobal.previous();
    if(local_history.indexOf(entry) == -1)
      menu->addAction(entry, receiver, slot);
    }

  if(nLocal > 0 && menu->actions().size() > nLocal)
    menu->insertSeparator(menu->actions()[nLocal]);
}


void PopulateHistoryMenu(
    QMenu *menu, QObject *receiver, const char *slot,
    GlobalUIModel *model, QString hist_category)
{
  HistoryManager *hm =
      model->GetDriver()->GetSystemInterface()->GetHistoryManager();

  QStringList hl = toQStringList(hm->GetLocalHistory(hist_category.toStdString()));
  QStringList hg = toQStringList(hm->GetGlobalHistory(hist_category.toStdString()));
  PopulateHistoryMenu(menu, receiver, slot, hl, hg);
}




/** Show a generic file save dialog with a history dropdown */
QString ShowSimpleSaveDialogWithHistory(
    GlobalUIModel *model, QString hist_category,
    QString window_title, QString file_title, QString file_pattern)
{
  HistoryManager *hm =
      model->GetDriver()->GetSystemInterface()->GetHistoryManager();

  QStringList hl = toQStringList(hm->GetLocalHistory(hist_category.toStdString()));
  QStringList hg = toQStringList(hm->GetGlobalHistory(hist_category.toStdString()));

  return SimpleFileDialogWithHistory::showSaveDialog(
        window_title, file_title, hl, hg, file_pattern);
}

/** Show a generic file open dialog with a history dropdown */
QString ShowSimpleOpenDialogWithHistory(
    GlobalUIModel *model, QString hist_category,
    QString window_title, QString file_title, QString file_pattern)
{
  HistoryManager *hm =
      model->GetDriver()->GetSystemInterface()->GetHistoryManager();

  QStringList hl = toQStringList(hm->GetLocalHistory(hist_category.toStdString()));
  QStringList hg = toQStringList(hm->GetGlobalHistory(hist_category.toStdString()));

  return SimpleFileDialogWithHistory::showOpenDialog(
        window_title, file_title, hl, hg, file_pattern);
}

bool SaveImageLayer(GlobalUIModel *model, ImageWrapperBase *wrapper,
                    LayerRole role, bool force_interactive,
                    QWidget *parent)
{
  // Create a model for saving the segmentation image via a wizard
  SmartPtr<ImageIOWizardModel> wiz_model =
      model->CreateIOWizardModelForSave(wrapper, role);

  // Interactive or not?
  if(force_interactive || wiz_model->GetSuggestedFilename().size() == 0)
    {
    // Execute the IO wizard
    ImageIOWizard wiz(parent);
    wiz.SetModel(wiz_model);
    wiz.exec();
    }
  else
    {
    try
      {
      QtCursorOverride curse(Qt::WaitCursor);
      wiz_model->SaveImage(wiz_model->GetSuggestedFilename());
      }
    catch(std::exception &exc)
      {
      ReportNonLethalException(
            parent, exc, "Image IO Error",
            QString("Failed to save image %1").arg(
              from_utf8(wiz_model->GetSuggestedFilename())));
      }
    }

  return wiz_model->GetSaveDelegate()->IsSaveSuccessful();
}

bool SaveWorkspace(GlobalUIModel *model, bool interactive, QWidget *widget)
{
  // Get the currently stored project name
  QString file_abs = from_utf8(model->GetGlobalState()->GetProjectFilename());

  // Prompt for a project filename if one was not provided
  if(interactive || file_abs.length() == 0)
    {
    // Use the dialog with history - to be consistent with other parts of SNAP
    QString file = ShowSimpleSaveDialogWithHistory(
          model, "Project", "Save Workspace",
          "Workspace File", "ITK-SNAP Workspace Files (*.itksnap)");

    // If user hits cancel, move on
    if(file.isNull())
      return false;

    // Make sure to get an absolute path, because the project needs that info
    file_abs = QFileInfo(file).absoluteFilePath();
    }

  // If file was provided, set it as the current project file
  try
    {
    model->GetDriver()->SaveProject(to_utf8(file_abs));
    return true;
    }
  catch(exception &exc)
    {
    ReportNonLethalException(widget, exc, "Error Saving Project",
                             QString("Failed to save project %1").arg(file_abs));
    return false;
    }
}

#include <QFileDialog>
#include <QFileInfo>

// MacOS bug workaround (sort-of, not sure it always helps) for opening a single
// file using the native browser. The default implementation QFileDialog::getOpenFileName
// does not always set the directory correctly
QString GetOpenFileNameBugFix(
    QWidget *parent,
    const QString &caption,
    const QString &user_file,
    const QString &filter)
{
  QFileDialog dialog(parent, caption);
  dialog.setFileMode(QFileDialog::ExistingFile);
  dialog.setAcceptMode(QFileDialog::AcceptOpen);

  if(user_file.length())
    {
    QFileInfo file_info(user_file);
    dialog.setDirectory(file_info.absolutePath() + "/");
    dialog.selectFile(file_info.fileName());
    }

  if(filter.length())
    dialog.setNameFilter(filter);

  if(dialog.exec() && dialog.selectedFiles().size())
    return dialog.selectedFiles().first();
  else
    return QString();
}
