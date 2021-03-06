/***************************************************************************
 *   Copyright (C) 2007 by Harm van Eersel                                 *
 *   devsciurus@xs4all.nl                                                  *
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
 *   51 Franklin Street, Fifth Floor, Boston, MA  02110-1301, USA.         *
 ***************************************************************************/

/** @file
 * This file is part of molsKetch and contains the lineWidthAction class.
 *
 * @author Hendrik Vennekate <HVennekate@gmx.de>
 * @since Lithium
 */

#ifndef LINEWIDTHACTION_H
#define LINEWIDTHACTION_H

#include "abstractrecursiveitemaction.h"

namespace Molsketch {

  class lineWidthAction : public abstractRecursiveItemAction
  {
    Q_OBJECT
  public:
    explicit lineWidthAction(MolScene *parent = 0);

  private:
    void execute() ;
  };

} // namespace

#endif // LINEWIDTHACTION_H
