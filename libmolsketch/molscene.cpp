/***************************************************************************
 *   Copyright (C) 2007-2008 by Harm van Eersel                            *
 *   Copyright (C) 2009 Tim Vandermeersch                                  *
 *   Copyright (C) 2009 by Nicola Zonta                                    *
 *                                                                         *
 *   This program is free software; you can redistribute it and/or modify  *
 *   it under the terms of the GNU General Public License as published by  *
 *   the Free Software Foundation; either version 2 of the License, or     *
 *   (at your option) any later version.                                   *
 *                                                                         *
 *   This program is distributed in the hope that it will be useful,       *
 *   but WITHOUT ANY WARRANTY; without even the implied warranty of        *
 *   MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the         *
 *   GNU General Public License for more details.                          *
 *                                                                         *
 *   You should have received a copy of the GNU General Public License     *
 *   along with this program; if not, write to the                         *
 *   Free Software Foundation, Inc.,                                       *
 *   59 Temple Place - Suite 330, Boston, MA  02111-1307, USA.             *
 ***************************************************************************/

#include <iostream>

#include <QGraphicsSceneMouseEvent>
#include <QPainter>
#include <QClipboard>
#include <QApplication>
#include <QListWidgetItem>
#include <QTableWidgetItem>
#include <QKeyEvent>
#include <QUndoStack>
#include <QMenu>
#include <QProcess>
#include <QDir>
#include <QLibrary>
#include <QMessageBox>
#include <QSvgGenerator>
#include <QPushButton>
#include <QBuffer>
#if QT_VERSION < 0x050000
#include <QtMath>
#include <QDesktopServices>
#else
#include <QtCore/qmath.h>
#include <QStandardPaths>
#endif
#include <QDebug>
#include <QDockWidget>
#include <QLabel>
#include <QMainWindow>
#include <QPair>
#include <QVBoxLayout>

#include "molscene.h"

#include "element.h"
#include "atom.h"
#include "bond.h"
#include "molecule.h"
#include "commands.h"
#include "mimemolecule.h"
#include "TextInputItem.h"
#include "math2d.h"
#include "grid.h"
#include "molview.h"

#include "arrow.h"
#include "actions/abstractitemaction.h"
#include "actions/genericaction.h"

#include "frame.h"
#include "scenesettings.h"
#include "textitem.h"

#ifdef QT_STATIC_BUILD
inline void initToolBarIcons() { Q_INIT_RESOURCE(toolicons); }
#endif

namespace Molsketch {

#ifdef QT_STATIC_BUILD
  void initToolBarIconsMsk() { initToolBarIcons(); }
#endif

  const QString MolScene::mouseWheelForCyclingTools = "mouse-wheel-cycle-tools";

  struct MolScene::privateData
  {
    QGraphicsRectItem *selectionRectangle;
    TextInputItem *inputItem;
    grid *Grid;
    MolScene *scene;
    QDockWidget *propertiesDock;
    QLabel *propertiesHelpLabel;

    QAction *bondLengthAction,
    *bondWidthAction,
    *arrowWidthAction,
    *frameLinewidthAction,
    *bondAngleAction,
    *atomSizeAction,
    *atomFontAction,
    *hydrogenVisibleAction,
    *carbonVisibleAction,
    *lonePairsVisibleAction,
    *autoAddHydrogenAction,
    *electronSystemsVisibleAction,
    *chargeVisibleAction,
    *defaultColorAction;
    SceneSettings *settings;

    graphicsItem* dragItem;

    QMap<QAction*, QPair<void (MolScene::*)(const bool&), bool (MolScene::*)() const> > booleanActions;
    void attachDockWidgetToMainWindow(MolScene* scene)
    {
      if (scene)
      {
        QMainWindow* mainWindow = qobject_cast<QMainWindow*>(scene->parent());
        if (mainWindow)
          mainWindow->addDockWidget(Qt::LeftDockWidgetArea, propertiesDock);
      }
    }

    privateData(MolScene* scene)
      : selectionRectangle(new QGraphicsRectItem),
        inputItem(new TextInputItem),
        Grid(new grid),
        scene(scene),
        propertiesDock(new QDockWidget(tr("Properties"))),
        propertiesHelpLabel(new QLabel(tr("Select single item to edit properties"), propertiesDock)),
        bondLengthAction(new QAction(tr("Bond length..."), scene)),
        bondWidthAction(new QAction(tr("Bond linewidth..."), scene)),
        arrowWidthAction(new QAction(tr("Arrow width..."), scene)),
        frameLinewidthAction(new QAction(tr("Bracket linewidth..."), scene)),
        bondAngleAction(new QAction(tr("Bond snap angle..."), scene)),
        atomSizeAction(new QAction(tr("Atom size..."), scene)),
        atomFontAction(new QAction(tr("Atom font..."), scene)),
        hydrogenVisibleAction(new QAction(tr("Hydrogens visible"), scene)),
        carbonVisibleAction(new QAction(tr("Carbons visible"), scene)),
        lonePairsVisibleAction(new QAction(tr("Lone pairs visible"), scene)),
        autoAddHydrogenAction(new QAction(tr("Auto add hydrogens"), scene)),
        electronSystemsVisibleAction(new QAction(tr("Electron systems visible"), scene)),
        chargeVisibleAction(new QAction(tr("Charges visible"), scene)),
        defaultColorAction(new QAction(tr("Default color..."), scene)),
        dragItem(0)
    {
      selectionRectangle->setPen(QPen(Qt::blue,0,Qt::DashLine));
      selectionRectangle->setZValue(INFINITY);
#define REGISTER_ACTION(CAPLETTER, LOWERLETTER, PROPNAME) booleanActions[LOWERLETTER##PROPNAME##Action] = qMakePair(&MolScene::set##CAPLETTER##PROPNAME, &MolScene::LOWERLETTER##PROPNAME);
      REGISTER_ACTION(H,h,ydrogenVisible)
      REGISTER_ACTION(C,c,arbonVisible)
      REGISTER_ACTION(L,l,onePairsVisible)
      REGISTER_ACTION(A,a,utoAddHydrogen)
      REGISTER_ACTION(E,e,lectronSystemsVisible)
      REGISTER_ACTION(C,c,hargeVisible)
      foreach(QAction* booleanAction, booleanActions.keys())
      {
        booleanAction->setCheckable(true);
        booleanAction->setChecked((scene->*booleanActions[booleanAction].second)());
        connect(booleanAction, SIGNAL(toggled(bool)), scene, SLOT(booleanPropertyChanged(bool)));
        connect(booleanAction, SIGNAL(toggled(bool)), scene, SLOT(updateAll()));
        connect(scene, SIGNAL(sceneRectChanged(QRectF)), scene, SLOT(updateGrid(QRectF)));
      }

      propertiesHelpLabel->setAlignment(Qt::AlignTop);
      propertiesHelpLabel->setDisabled(true);
      propertiesDock->setWidget(propertiesHelpLabel);
      attachDockWidgetToMainWindow(scene);
    }

    QMenu* contextSubMenu()
    {
      QMenu* menu = new QMenu(tr("Scene properties"));
      menu->addActions(booleanActions.keys());
      return menu;
    }

    ~privateData()
    {
//      if (inputItem && !inputItem->scene()) // TODO compare with this scene
//        delete inputItem; // TODO should clean up this item...
//      delete selectionRectangle; // TODO why?
      propertiesDock->setWidget(propertiesHelpLabel);
      delete propertiesDock;
      if (!Grid->scene()) delete Grid;
    }

    bool gridOn()const { return Grid->scene(); }

    void setPropertiesWidget(graphicsItem* item) {
      QWidget* oldWidget = propertiesDock->widget();
      QWidget* newWidget = item ? item->getPropertiesWidget() : propertiesHelpLabel;
      propertiesDock->setWidget(newWidget);
      if (oldWidget != newWidget && oldWidget != propertiesHelpLabel)
        delete oldWidget;
    }

    void moveDragItem(QGraphicsSceneDragDropEvent* event) {
      if (!dragItem) return;
      dragItem->moveItemBy(event->scenePos() - dragItem->boundingRect().center());
    }
  };

  using namespace Commands;


  //////////////////////////////////////////////////////////////////////////////
  //
  // Constructor & destructor
  //
  //////////////////////////////////////////////////////////////////////////////

  void MolScene::initialize(SceneSettings* settings)
  {
    //Initializing properties
    m_editMode = MolScene::DrawMode;
    m_renderMode = RenderLabels;

    // Prepare undo m_stack
    m_stack = new QUndoStack(this);
    connect(m_stack, SIGNAL(indexChanged(int)), this, SIGNAL(documentChange()));
    connect(m_stack, SIGNAL(indexChanged(int)), this, SLOT(update()));
    connect(m_stack, SIGNAL(indexChanged(int)), this, SLOT(updateAll())) ;
    connect(this, SIGNAL(selectionChanged()), this, SLOT(selectionSlot()));

    // Set initial size
    QRectF sizerect(-5000,-5000,10000,10000);
    setSceneRect(sizerect);
    // TODO - add text item
    // - subclass QGraphicsTextItem?
    // - make movable
    // - make serializable
    // - make readable
    // - create action
    // (- extract parent item from graphicsItem)
//    QGraphicsTextItem *textItem = new QGraphicsTextItem();
//    textItem->setHtml("some <b>text</b> example");
//    textItem->setTextInteractionFlags(Qt::TextEditorInteraction);
//    addItem(textItem);

    d->settings = settings;
  }

  MolScene::MolScene(QObject* parent)
    : QGraphicsScene(parent),
      d(new privateData(this))
  {
    initialize(new SceneSettings(this));
  }

  MolScene::MolScene(SceneSettings* settings, QObject *parent)
    : QGraphicsScene(parent),
      d(new privateData(this))
  {
    initialize(settings);
  }

  MolScene::~MolScene()
  {
    clear();
  }

  QFont MolScene::getAtomFont() const {
    return d->settings->getAtomFont();
  }

  QString MolScene::mimeType()
  {
    return "molecule/molsketch";
  }
  // Commands

  void MolScene::alignToGrid() // TODO this isn't used (and not correct)
  {
    setGrid(true);
        m_stack->beginMacro(tr("aligning to grid"));
        foreach(QGraphicsItem* item,items())
          if (item->type() == Molecule::Type)
                m_stack->push(MoveItem::relative(item,toGrid(item->scenePos()) - item->scenePos()));
        m_stack->endMacro();
        update();
  }

  void MolScene::setEditMode(int mode)
  {
    // TODO
        // Set the new edit mode and signal other components
        m_editMode = mode;
        emit editModeChange( mode );
  }

  void MolScene::cut()
  {
        /* TODO Using the desktop clipboard*/
        // Check if something is selected
        if (selectedItems().isEmpty()) return;

        // Then do a copy
        copy();

        // Finally delete the selected items
        m_stack->beginMacro(tr("cutting items"));
        foreach (QGraphicsItem* item, selectedItems())
          if (item->type() == Molecule::Type) m_stack->push(new DelItem(item));
        m_stack->endMacro();
  }

  void MolScene::copy() // TODO this doesn't seem to work
  {
        // Check if something is selected
        if (selectedItems().isEmpty()) return;

        /* TODO Using the desktop clipboard */
        // Access the clipboard
        QClipboard* clipboard = qApp->clipboard();

        // Calculate total boundingrect
        QRectF totalRect;
        foreach(QGraphicsItem* item, selectedItems())
        {
          QRectF itemRect = item->boundingRect();
          itemRect.translate(item->scenePos());
          totalRect |= itemRect;
        }
        // Add to internal clipboard
        foreach(QGraphicsItem* item, m_clipItems) delete item;
        m_clipItems.clear();
        foreach(QGraphicsItem* item, selectedItems())
          if (item->type() == Molecule::Type)
                m_clipItems.append(new Molecule(dynamic_cast<Molecule*>(item)));

        // Clear selection
        QList<QGraphicsItem*> selList(selectedItems());
        clearSelection();

        // Choose the datatype
        //   clipboard->setText("Test");
        clipboard->setImage(renderImage(totalRect));
        //   clipboard->mimeData( );

        // Restore selection
        foreach(QGraphicsItem* item, selList) item->setSelected(true);

        // Emit paste available signal
        emit pasteAvailable(!m_clipItems.isEmpty());
  }

  void MolScene::paste()
  {
        // Access the clipboard
        //   QClipboard* clipboard = qApp->clipboard();
        /* TODO Using the system clipboard*/

        // Paste all items on the internal clipboard
        m_stack->beginMacro(tr("pasting items"));
        foreach(Molecule* item, m_clipItems) m_stack->push(new AddItem(new Molecule(item),this));
        m_stack->endMacro();
  }

#ifdef QT_DEBUG
  void MolScene::debugScene()
  {
    QtMessageHandler originalMessageHander = qInstallMessageHandler(0);
    qDebug() << "================= Scene debug =================";
    foreach(QGraphicsItem *item, items())
    {
      qDebug() << "Item:" << item
               << "Type:" << item->type()
               << "Pos:"  << item->pos()
               << "Scene Pos:" << item->scenePos()
               << "Bounds:" << item->boundingRect()
               << "Children:" << item->childItems() ;
    }
    qDebug() << "============== Undo stack debug ===============";
    qDebug() << "position:" << stack()->index();
    for (int i = 0 ; i < stack()->count() ; ++i)
    {
      const QUndoCommand* command = stack()->command(i);
      qDebug() << i << command << command->id() << command->text();
    }
    qDebug() << "===============================================";
    qInstallMessageHandler(originalMessageHander);
  }
#endif

  void MolScene::clear()
  {
    clearSelection();
    m_stack->clear();
    delete d;
    QGraphicsScene::clear();
    d = new privateData(this);
  }

  QImage MolScene::renderMolToImage (Molecule *mol)
  {
                QRectF rect = mol ->boundingRect();
                QImage image(int(rect.width()),int(rect.height()),QImage::Format_RGB32);
                image.fill(QColor("white").rgb());

                // Creating and setting the painter
                QPainter painter(&image);
                painter.setRenderHint(QPainter::Antialiasing);

                // Rendering in the image and saving to file
                render(&painter,QRectF(0,0,rect.width(),rect.height()),QRectF (mol ->mapToScene (rect.topLeft ()), mol ->mapToScene (rect.bottomRight ())));
                return image;
  }

  QByteArray MolScene::toSvg()
  {
    QList<QGraphicsItem*> selection(selectedItems());
    clearSelection();
    QByteArray ba;
    QBuffer buffer(&ba);
    buffer.open(QBuffer::WriteOnly);
    QSvgGenerator svgGenerator;
    svgGenerator.setTitle(tr("MolsKetch Drawing"));
    QRectF bounds(itemsBoundingRect());
    svgGenerator.setSize(bounds.size().toSize()); // TODO round up
    svgGenerator.setViewBox(bounds);
    svgGenerator.setOutputDevice(&buffer);
    QPainter painter;
    painter.begin(&svgGenerator);
    render(&painter, bounds, bounds);
    painter.end();
    buffer.close();
    // TODO reselect items
    return ba;
  }


  QImage MolScene::renderImage(const QRectF &rect)
  {
        // Creating an image
        QImage image(int(rect.width()),int(rect.height()),QImage::Format_RGB32);
        image.fill(QColor("white").rgb());

        // Creating and setting the painter
        QPainter painter(&image);
        painter.setRenderHint(QPainter::Antialiasing);

        // Rendering in the image and saving to file
        render(&painter,QRectF(0,0,rect.width(),rect.height()),rect);

        return image;
  }

  void MolScene::addMolecule(Molecule* mol)
  {
        Q_CHECK_PTR(mol);
        if (!mol) return;
        m_stack->beginMacro(tr("add molecule"));
        m_stack->push(new AddItem(mol,this));
        if (mol->canSplit()) {
          for(Molecule* molecule : mol->split())
            m_stack->push(new AddItem(molecule, this));
          m_stack->push(new DelItem(mol));
        }
        m_stack->endMacro();
  }

  void MolScene::selectAll()
  {
        // Switch to move mode to make selection posible
        setEditMode(MolScene::MoveMode);

        // Clear any previous selection
        clearSelection();

        // Mark all atoms as selected
        foreach (QGraphicsItem* item, items())
        {
          if (item->type() == Molecule::Type || item->type() == Arrow::Type)
                item->setSelected(true);
        }
  }


  void MolScene::setHoverRect( QGraphicsItem* item )
  {
        if (item)
        {
          m_hoverRect->setPath(item->shape());
          m_hoverRect->setPos(item->scenePos());
          addItem(m_hoverRect);
        }
        else
          removeItem(m_hoverRect);
  }

  int MolScene::editMode() const
  {
        return m_editMode;
  }

  MolScene::RenderMode MolScene::renderMode() const
  {
        return m_renderMode;
  }

  void MolScene::setRenderMode(MolScene::RenderMode mode)
  {
        m_renderMode = mode;
  }

  QPointF MolScene::toGrid(const QPointF &position) // TODO obsolete
  {
        QPointF p = position;
        int factor = 40;
        p.rx() = floor(p.x() / factor) * factor;
        p.ry() = floor(p.y() / factor) * factor;

        return p;
  }

  QPointF MolScene::snapToGrid(const QPointF &point, bool force)
  {
    if (!d->gridOn() && !force) return point;
    return d->Grid->alignPoint(point);
  }

  bool MolScene::snappingToGrid() const
  {
    return d->gridOn();
  }

  bool MolScene::cyclingByMouseWheelEnaled() const
  {
    if (d->settings->getMouseWheelMode() == SceneSettings::Unset) {
      QMessageBox promptForMouseWheelUsage;
      promptForMouseWheelUsage.setWindowTitle(tr("Mouse wheel configuration"));
      promptForMouseWheelUsage.setText(tr("Mouse wheel use has not been configured."
                                          "Should the wheel be used to zoom, "
                                          "or to cycle tool settings?"));
      QAbstractButton *zoomButton = promptForMouseWheelUsage.addButton(tr("Use to zoom"), QMessageBox::YesRole);
      QAbstractButton *cycleButton = promptForMouseWheelUsage.addButton(tr("Use to cycle tool options"), QMessageBox::NoRole);
      promptForMouseWheelUsage.exec();
      if (promptForMouseWheelUsage.clickedButton() == zoomButton)
        d->settings->setMouseWheelMode(SceneSettings::Zoom);
      if (promptForMouseWheelUsage.clickedButton() == cycleButton)
        d->settings->setMouseWheelMode(SceneSettings::CycleTools);
    }
    return d->settings->getMouseWheelMode() == SceneSettings::CycleTools;
  }

  void MolScene::setGrid(bool on)
  {
    if (on) addItem(d->Grid);
    else removeItem(d->Grid);
  }

  XmlObjectInterface *MolScene::produceChild(const QString &childName, const QString &type)
  {
    XmlObjectInterface *object = 0 ;
    if ("frame" == childName) object = new Frame;
    if (childName == "molecule") // TODO move those names to their classes.
      object = new Molecule;
    if (childName == "arrow")
      object = new Arrow;
    if (childName == "object")
    {
      if (type == "ReactionArrow") object = new Arrow ;
      if (type == "MechanismArrow") object = new Arrow ;
    }
    if ("textItem" == childName) object = new TextItem;
    if (QGraphicsItem* item = dynamic_cast<QGraphicsItem*>(object)) // TODO w/o dynamic_cast
      addItem(item) ;
    return object ;
  }

  QList<const XmlObjectInterface *> MolScene::children() const
  {
        QList<const XmlObjectInterface*> childrenList ;
        foreach(QGraphicsItem* item, items())
        {
          if (item && item->parentItem()) continue;
          XmlObjectInterface *object = dynamic_cast<XmlObjectInterface*>(item);
          if (object) childrenList << object;
        }
        return childrenList ;
  }

  void MolScene::readAttributes(const QXmlStreamAttributes &attributes)
  {
    foreach(const QXmlStreamAttribute& attribute, attributes)
      setProperty(attribute.name().toLatin1(), attribute.value().toString());
    clear();
  }

  QXmlStreamAttributes MolScene::xmlAttributes() const
  {
    QXmlStreamAttributes attributes;
    foreach (const QByteArray& name, dynamicPropertyNames()) // TODO handle int, double, bool appropriately
      attributes.append(name, property(name).toString());
    return attributes;
  }

  Molecule* MolScene::moleculeAt(const QPointF &pos)
  {
        // Check if there is a molecule at this position
        foreach(QGraphicsItem* item,items(pos))
          if (item->type() == Molecule::Type) return dynamic_cast<Molecule*>(item);

        // Else return NULL
        return 0;

  }

  TextInputItem *MolScene::inputItem()
  {
    return d->inputItem;
  }

  QMenu *MolScene::sceneMenu() const
  {
    return d->contextSubMenu();
  }

  QList<Atom *> MolScene::atoms() const
  {
    QList<Atom*> result;
    foreach(auto item, items())
    {
      Atom* atom = dynamic_cast<Atom*>(item);
      if (atom) result << atom;
    }
    return result;
  }

  void MolScene::selectionSlot()
  {
    foreach(AbstractItemAction* itemAction, findChildren<AbstractItemAction*>())
      itemAction->setItems(selectedItems());
    graphicsItem* currentItem = 0; // TODO check if focusItem is what we actually want here
    if (selectedItems().size() == 1) currentItem = dynamic_cast<graphicsItem*>(selectedItems().first());
    d->setPropertiesWidget(currentItem);
  }

  void MolScene::booleanPropertyChanged(bool newValue)
  {
    QAction* action = dynamic_cast<QAction*>(sender());
    if (!d->booleanActions.contains(action)) return;
    (this->*d->booleanActions.value(action).first)(newValue);
  }

  void MolScene::updateGrid(const QRectF& newSceneRect)
  {
    d->Grid->update(newSceneRect);
  }

  Atom* MolScene::atomAt(const QPointF &pos) // TODO consider replacing with itemAt()
  {
    foreach(Atom* atom, atoms())
      if (atom->scenePos() == pos)
        return atom;
    return 0;
  }

  Atom* MolScene::atomNear(const QPointF &pos, qreal tolerance)
  {
    Atom* pickedAtom = atomAt(pos);
    if (pickedAtom) return pickedAtom;
    foreach(auto atom, atoms())
    {
      qreal newDistance = QLineF(atom->scenePos(), pos).length();
      if (newDistance < tolerance)
      {
          pickedAtom = atom;
          tolerance = newDistance;
      }
    }
    return pickedAtom;
  }

  Bond* MolScene::bondAt(const QPointF &pos)
  {
        foreach( QGraphicsItem* item, items(pos))
          if (item->type() == Bond::Type) return dynamic_cast<Bond*>(item);

        return 0;
  }



  // Event handlers

  bool MolScene::event(QEvent* event)
  {
        // Execute default behaivior
        bool accepted = QGraphicsScene::event(event);

        // Check whether copying is available
        if ((event->type() == QEvent::GraphicsSceneMouseRelease) || (event->type() == QEvent::KeyRelease))
          emit copyAvailable(!selectedItems().isEmpty());

        // Execute default behavior
        return accepted;
  }


  void MolScene::updateAll()
  {
    invalidate() ;
    update() ;
  }

  void MolScene::keyPressEvent(QKeyEvent* keyEvent)
  {
    // execute default behaviour (needed for text tool)
    keyEvent->ignore();
    QGraphicsScene::keyPressEvent(keyEvent);
    update();
    if (keyEvent->isAccepted()) return;
    if (keyEvent->key() == Qt::Key_Escape)
    {
        keyEvent->accept();
        clearSelection();
        for (genericAction* action : sceneActions())
          if (action->isChecked())
            action->setChecked(false);
    }
  }

  void MolScene::contextMenuEvent(QGraphicsSceneContextMenuEvent *event)
  {
        QMenu contextMenu;
        qDebug() << "context menu";
//        sceneMenu->setParent(&contextMenu);
//        contextMenu.addMenu(sceneMenu);
        qDebug() << "Generated menu";
        foreach(QGraphicsItem* qgItem, selectedItems())
        {
          graphicsItem *item = dynamic_cast<graphicsItem*>(qgItem);
          if (!item) continue;
          item->prepareContextMenu(&contextMenu);
        }

        qDebug() << "-------- context menu for no of items:" << selectedItems().size();
        if (contextMenu.actions().empty())
        {
          d->contextSubMenu()->exec(event->screenPos());
          event->accept();
          return;
        }
        contextMenu.addMenu(d->contextSubMenu()); // TODO memory leak

        contextMenu.exec(event->screenPos());
        event->accept();
  }

  void MolScene::mousePressEvent(QGraphicsSceneMouseEvent *event)
  {
    event->ignore();
    QGraphicsScene::mousePressEvent(event);
    if (event->isAccepted()) return;
    if (event->button() != Qt::LeftButton || event->modifiers()) return;
    d->selectionRectangle->setRect(QRectF(event->scenePos(), event->scenePos()));
    addItem(d->selectionRectangle);
    d->selectionRectangle->show();
    event->accept();
  }

  void MolScene::mouseMoveEvent(QGraphicsSceneMouseEvent *event)
  {
    event->ignore();
    QGraphicsScene::mouseMoveEvent(event);
    if (event->isAccepted()) return;
    if (!d->selectionRectangle->scene()) return;
    if (!(event->buttons() & Qt::LeftButton)) return;
    d->selectionRectangle->setRect(QRectF(event->buttonDownScenePos(Qt::LeftButton), event->scenePos()));
    QPainterPath selectArea;
    selectArea.addRect(d->selectionRectangle->rect());
    setSelectionArea(selectArea);
    event->accept();
  }

  void MolScene::mouseReleaseEvent(QGraphicsSceneMouseEvent *event)
  {
    event->ignore();
    QGraphicsScene::mouseReleaseEvent(event);
    if(event->isAccepted()) return;
    if (event->button() != Qt::LeftButton) return;
    removeItem(d->selectionRectangle);
    event->accept();
  }

  void MolScene::dragEnterEvent(QGraphicsSceneDragDropEvent *event)
  {
    if (!event->mimeData() || !event->mimeData()->hasFormat(mimeType())) return;
    if (!event->proposedAction() == Qt::CopyAction) return;
    event->accept();
    for (genericAction* action : sceneActions())
      action->setChecked(false);
    d->dragItem = new Molecule;
    QXmlStreamReader reader(event->mimeData()->data(mimeType()));
    reader >> *(d->dragItem);
    d->moveDragItem(event);
    addItem(d->dragItem);
    updateAll();
  }

  void MolScene::dragLeaveEvent(QGraphicsSceneDragDropEvent *event)
  {
    if (!d->dragItem) return;
    removeItem(d->dragItem);
    delete d->dragItem;
    d->dragItem = 0;
    event->accept();
  }

  void MolScene::dropEvent(QGraphicsSceneDragDropEvent *event)
  {
    if (!d->dragItem) return;
    event->accept();
    stack()->push(new Commands::AddItem(d->dragItem, this, tr("insert molecule")));
  }

  void MolScene::dragMoveEvent(QGraphicsSceneDragDropEvent *event)
  {
    qDebug() << "drag moving:" << event->proposedAction();
    if (!d->dragItem) return;
    d->moveDragItem(event);
    event->accept();
  }

  void MolScene::wheelEvent(QGraphicsSceneWheelEvent *event)
  {
    foreach(QGraphicsView* vp, views()) // TODO track back event to originator
    {
      MolView *mvp = qobject_cast<MolView*>(vp);
      if (mvp)
        mvp->scaleView(pow((double)2, -event->delta() / MOUSEWHEELDIVIDER));
    }
  }


  QUndoStack * MolScene::stack()
  {
        return m_stack;
  }

  QList<genericAction *> MolScene::sceneActions() const
  {
    QList<genericAction *> actions;
#if QT_VERSION < 0x050000
        QList<genericAction*> allActionChildren = findChildren<genericAction*>() ;
        std::copy_if(allActionChildren.begin(), allActionChildren.end(), std::back_inserter(actions),
                     [&](QAction* a) { return a->parent() == this; });
#else
        actions = findChildren<genericAction*>(QString(), Qt::FindDirectChildrenOnly);
#endif

        return actions;
  }

  template <class T>
  T *MolScene::itemNear(const QPointF &pos, qreal tolerance) { // TODO unit test
    qreal minDistance = tolerance;
    T *result = nullptr;
    for(QGraphicsItem* item : items(QRectF(pos.x()-tolerance, pos.y()-tolerance, tolerance, tolerance))) {
      if (!(QLineF(item->scenePos(), pos).length() < minDistance)) continue;
      if (T *itemPointer = dynamic_cast<T*>(item)) result = itemPointer;
    }
    return result;
  }

} // namespace