/*
 * Copyright (C) 2009 Nokia Corporation and/or its subsidiary(-ies)
 *
 * This library is free software; you can redistribute it and/or
 * modify it under the terms of the GNU Library General Public
 * License as published by the Free Software Foundation; either
 * version 2 of the License, or (at your option) any later version.
 *
 * This library is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the GNU
 * Library General Public License for more details.
 *
 * You should have received a copy of the GNU Library General Public License
 * along with this library; see the file COPYING.LIB.  If not, write to
 * the Free Software Foundation, Inc., 51 Franklin Street, Fifth Floor,
 * Boston, MA 02110-1301, USA.
 *
 */
#ifndef QtFallbackWebPopup_h
#define QtFallbackWebPopup_h

#include "QtAbstractWebPopup.h"
#include <QComboBox>

namespace WebCore {

class QtFallbackWebPopup : private QComboBox, public QtAbstractWebPopup {
    Q_OBJECT
public:
    QtFallbackWebPopup(PopupMenuClient* client);

    virtual void show(const QRect& geometry, int selectedIndex);
    virtual void hide() { hidePopup(); }

private slots:
    void activeChanged(int);

private:
    bool m_popupVisible;

    void populate();

    virtual void showPopup();
    virtual void hidePopup();
};

}

#endif // QtFallbackWebPopup_h
