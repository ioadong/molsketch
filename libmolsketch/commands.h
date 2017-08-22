/***************************************************************************
 *   Copyright (C) 2007 by Harm van Eersel                                 *
 *   devsciurus@xs4all.nl                                                  *
 *   Copyright (C) 2015 Hendrik Vennekate                                  *
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

/** @file
 * This file is part of molsKetch and contains the command classes used in
 * the Qt Redo/Undo system. Each class represents an action on the scene and
 * contains the code to redo/undo that action.
 *
 * @author Harm van Eersel <devsciurus@xs4all.nl>
 * @since Deuterium
 */


#ifndef commands_H
#define commands_H

#include <QUndoCommand>
#include <QPointF>
#include <functional>

class QGraphicsItem;
class QGraphicsScene;
class QTransform;

#include "bond.h"
#include "arrow.h"
#include "frame.h"
#include "molecule.h"
#include "molscene.h"

namespace Molsketch {

  class Atom;
  class Molecule;
  class MolScene;
  class Residue;

  namespace Commands { // TODO make null safe and rework

    enum CommandId {
      BondTypeId = 1,
      ArrowTypeId,
      ArrowPropertiesId,
      FrameTypeId,
      ArrowSplineId,
      AtomChargeId,
      AtomImplicitHydrogensId,
      MoveItemId,
      MoleculeNameId,
    };


    template<class ItemType, class OwnType, int CommandId = -1>
    class Command : public QUndoCommand { // TODO unit test
      ItemType* item;
    protected:
      virtual ItemType* getItem() const { return item; }
    public:
      explicit Command(const QString& text = "", QUndoCommand* parent = 0)
        : Command(0, text, parent) {}
      explicit Command(ItemType* item, const QString& text = "", QUndoCommand* parent = 0)
        : QUndoCommand(text, parent), item(item) {}
      void undo() override { redo(); }
      int id() const override { return CommandId; }
      bool mergeWith(const QUndoCommand *other) override {
        auto otherCommand = dynamic_cast<const OwnType*>(other);
        if (!otherCommand) return false;
        if (otherCommand->getItem()!= this->getItem()) return false;
        return true;
      }
      void execute()
      {
        QGraphicsItem *item = getItem();
        MolScene *scene = nullptr;
        QUndoStack *stack = nullptr;
        if (item) scene = dynamic_cast<MolScene*>(item->scene());
        if (scene) stack = scene->stack();

        if (stack) stack->push(this);
        else {
          redo();
          delete this;
        }
      }
    };

    template<class ItemClass,
             class ItemPropertyType,
             void (ItemClass::*setFunction)(const ItemPropertyType&),
             ItemPropertyType (ItemClass::*getFunction)()const,
             int CommandId = -1>
    class setItemPropertiesCommand : public Command<ItemClass, setItemPropertiesCommand<ItemClass, ItemPropertyType, setFunction, getFunction, CommandId>, CommandId>
    {
    private:
      ItemPropertyType type;
      typedef setItemPropertiesCommand<ItemClass, ItemPropertyType, setFunction, getFunction, CommandId> ownType;
    public:
      setItemPropertiesCommand(ItemClass *item, ItemPropertyType newType, const QString& text = "", QUndoCommand *parent = 0)
        : Command<ItemClass, ownType, CommandId>(item, text, parent),
          type(newType){}
      void redo() override {
        ItemPropertyType temp = (this->getItem()->*getFunction)();
        (this->getItem()->*setFunction)(type);
        type = temp;
        this->getItem()->update();
      }
    };

    template<class ItemClass,
             class ItemProperty,
             void (ItemClass::*setter)(ItemProperty),
             ItemProperty (ItemClass::*getter)() const,
             int CommandId = -1>
    class SetItemProperty : public Command<ItemClass, SetItemProperty<ItemClass, ItemProperty, setter, getter, CommandId>, CommandId >
    {
    private:
      ItemProperty value;
      typedef SetItemProperty<ItemClass, ItemProperty, setter, getter, CommandId> ownType;
    public:
      SetItemProperty(ItemClass *item, ItemProperty value, const QString& text = "", QUndoCommand *parent = 0)
        : Command<ItemClass, ownType, CommandId>(item, text, parent), value(value) {}
      void redo() override {
        ItemProperty temp = (this->getItem()->*getter)();
        (this->getItem()->*setter)(value);
        value = temp;
        this->getItem()->update();
      }
    };

    class AddAtom : public Command<Molecule, AddAtom> {
    public:
      AddAtom(Atom* newAtom, Molecule* molecule, const QString & text = "");
      ~AddAtom();
      virtual void undo() override;
      virtual void redo() override;
    private:
      Atom* atom;
      Molecule* molecule;
    };

    class DelAtom : public QUndoCommand
    {
    public:
      DelAtom(Atom* delAtom, const QString & text = "");
      virtual ~DelAtom();
      virtual void undo() override;
      virtual void redo() override;
    private:
      bool m_undone;
      Atom* m_atom;
      Molecule* m_molecule;
      QList<Bond*> m_bondList;
    };

    class AddBond : public QUndoCommand
    {
    public:
      AddBond(Bond* newBond, const QString & text = "");
      ~AddBond();
      virtual void undo() override;
      virtual void redo() override;
    private:
      bool m_undone;
      Bond* m_bond;
      Molecule* m_mol;
    };

    class DelBond : public  QUndoCommand
    {
    public:
      DelBond(Bond* delBond, const QString & text = "");
      virtual ~DelBond();
      virtual void undo() override;
      virtual void redo() override;
    private:
      bool m_undone;
      Bond* m_bond;
      Molecule* m_mol;
    };

    typedef setItemPropertiesCommand<Bond, Bond::BondType, &Bond::setType, &Bond::bondType, BondTypeId> SetBondType;
    typedef setItemPropertiesCommand<Arrow, Arrow::ArrowType, &Arrow::setArrowType, &Arrow::getArrowType, ArrowTypeId> SetArrowType;
    typedef setItemPropertiesCommand<Arrow, Arrow::Properties, &Arrow::setProperties, &Arrow::getProperties, ArrowPropertiesId> setArrowProperties;
    typedef setItemPropertiesCommand<Frame, QString, &Frame::setFrameString, &Frame::frameString, FrameTypeId> SetFrameTypeString;
    typedef setItemPropertiesCommand<Arrow, bool, &Arrow::setSpline, &Arrow::getSpline, ArrowSplineId> setArrowSplineCommand;
    typedef setItemPropertiesCommand<Atom, int, &Atom::setCharge, &Atom::charge, AtomChargeId> setAtomChargeCommand;
    typedef setItemPropertiesCommand<Atom, int, &Atom::setNumImplicitHydrogens, &Atom::numImplicitHydrogens, AtomImplicitHydrogensId> setImplicitHydrogensCommand;
    typedef SetItemProperty<QGraphicsItem, QGraphicsItem*, &QGraphicsItem::setParentItem, &QGraphicsItem::parentItem> SetParentItem;
    typedef setItemPropertiesCommand<Atom, QString, &Atom::setElement, &Atom::element> ChangeElement;
    typedef setItemPropertiesCommand<Molecule, QString, &Molecule::setName, &Molecule::getName, MoleculeNameId> ChangeMoleculeName;
    typedef setItemPropertiesCommand<graphicsItem, qreal, &graphicsItem::setRelativeWidth, &graphicsItem::relativeWidth> changeRelativeWidth;
    typedef setItemPropertiesCommand<graphicsItem, QColor, &graphicsItem::setColor, &graphicsItem::getColor> changeColor;

    class SwapBondAtoms : public setItemPropertiesCommand<Bond, QPair<Atom*, Atom*>, &Bond::setAtoms, &Bond::atoms> {
    public:
      SwapBondAtoms(Bond *bond, const QString &text = "", QUndoCommand* parent = 0)
        : setItemPropertiesCommand(bond, qMakePair(bond->endAtom(), bond->beginAtom()), text, parent ) {}
    };

    class AddItem : public QUndoCommand
    {
    public:
      AddItem(QGraphicsItem* newItem, MolScene* addScene, const QString & text = "");
      ~AddItem();
      virtual void undo();
      virtual void redo();
    private:
      QGraphicsItem* m_item;
      MolScene* m_scene;
    };

    class DelItem : public QUndoCommand
    {
    public:
      DelItem(QGraphicsItem* delItem, const QString & text = "");
      ~DelItem();
      virtual void undo();
      virtual void redo();
    private:
      QGraphicsItem* m_item;
      MolScene* m_scene;
    };

    class MoveItem : public setItemPropertiesCommand<QGraphicsItem, QPointF, &QGraphicsItem::setPos, &QGraphicsItem::pos, MoveItemId>
    {
    public:
      virtual void redo() override;
      static MoveItem* relative(QGraphicsItem* item, const QPointF& shift, const QString& text = "");
      static MoveItem* absolute(QGraphicsItem* item, const QPointF& newPos, const QString& text = "");
    private:
      MoveItem(QGraphicsItem* item, const QPointF& newPosition, const QString& text = "", QUndoCommand *parent = 0);
    };

  } // namespace Commands

} // namespace Molsketch

#endif