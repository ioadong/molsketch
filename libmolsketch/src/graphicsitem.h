#ifndef GRAPHICSITEM_H
#define GRAPHICSITEM_H

#include <QGraphicsItem>
#include "abstractxmlobject.h"

#if QT_VERSION < 0x050000
#define GRAPHICSSCENEHEADER , QGraphicsScene * scene = 0
#define GRAPHICSSCENESOURCE , QGraphicsScene *scene
#define GRAPHICSSCENEINIT , scene
#else
#define GRAPHICSSCENEHEADER
#define GRAPHICSSCENESOURCE
#define GRAPHICSSCENEINIT
#endif

class QUndoCommand ;

namespace Molsketch {

  class MolScene ;

  class coordinateItem: public abstractXmlObject
  {
  public:
    virtual QVector<QPointF> coordinates() const = 0;
    virtual void setCoordinates(const QVector<QPointF>& c) = 0;
  };

  class graphicsItem : public QGraphicsItem, public coordinateItem
  {
  public:

    enum Types {
      MoleculeType = QGraphicsItem::UserType + 1,
      AtomType = QGraphicsItem::UserType + 2,
      BondType = QGraphicsItem::UserType + 3,
      ResidueType = QGraphicsItem::UserType + 4,
      TextInputType = QGraphicsItem::UserType + 5,
      ReactionArrowType = QGraphicsItem::UserType + 6,
      MechanismArrowType = QGraphicsItem::UserType + 7
    };

    graphicsItem(QGraphicsItem* parent = 0 GRAPHICSSCENEHEADER);
    graphicsItem(const graphicsItem& other GRAPHICSSCENEHEADER);
    // TODO XML i/o
    void setColor(const QColor &color) ;
    QColor getColor() const ;
    void setRelativeWidth(const double& w) ;
    qreal lineWidth() const ;
    qreal relativeWidth() const ;
  protected:
    void readAttributes(const QXmlStreamAttributes &attributes) ;
    QXmlStreamAttributes xmlAttributes() const ;
    virtual void readGraphicAttributes(const QXmlStreamAttributes& attributes) { Q_UNUSED(attributes)}
    virtual QXmlStreamAttributes graphicAttributes() const { return QXmlStreamAttributes() ; }
    /**
     * Attempt to push a command onto the undo stack. If none is available, execute the command and dispose of it.
     */
    void attemptUndoPush(QUndoCommand* command) ;
  private:
    QColor m_color ;
    qreal lineWidthScaling ;
    virtual qreal sceneLineWidth(MolScene* scene) const ;
    void contextMenuEvent(QGraphicsSceneContextMenuEvent *event) ;
  };

  class arrowGraphicsItem : public graphicsItem
  {
  public:
    arrowGraphicsItem(QGraphicsItem* parent = 0 GRAPHICSSCENEHEADER ) ;
    QString xmlName() const { return "object" ; }
  private:
    qreal sceneLineWidth(MolScene *scene) const ;
  };

} // namespace

#endif // GRAPHICSITEM_H
