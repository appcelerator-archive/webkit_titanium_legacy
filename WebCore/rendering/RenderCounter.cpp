/**
 * Copyright (C) 2004 Allan Sandfeld Jensen (kde@carewolf.com)
 * Copyright (C) 2006, 2007 Apple Inc. All rights reserved.
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

#include "config.h"
#include "RenderCounter.h"

#include "CounterNode.h"
#include "Document.h"
#include "HTMLNames.h"
#include "HTMLOListElement.h"
#include "RenderListItem.h"
#include "RenderListMarker.h"
#include "RenderStyle.h"
#include <wtf/StdLibExtras.h>

namespace WebCore {

using namespace HTMLNames;

typedef HashMap<RefPtr<AtomicStringImpl>, CounterNode*> CounterMap;
typedef HashMap<const RenderObject*, CounterMap*> CounterMaps;

static CounterNode* makeCounterNode(RenderObject*, const AtomicString& counterName, bool alwaysCreateCounter);

static CounterMaps& counterMaps()
{
    DEFINE_STATIC_LOCAL(CounterMaps, staticCounterMaps, ());
    return staticCounterMaps;
}

static inline RenderObject* previousSiblingOrParent(RenderObject* object)
{
    if (RenderObject* sibling = object->previousSibling())
        return sibling;
    return object->parent();
}

static bool planCounter(RenderObject* object, const AtomicString& counterName, bool& isReset, int& value)
{
    ASSERT(object);

    // Real text nodes don't have their own style so they can't have counters.
    // We can't even look at their styles or we'll see extra resets and increments!
    if (object->isText() && !object->isBR())
        return false;

    RenderStyle* style = object->style();
    ASSERT(style);

    if (const CounterDirectiveMap* directivesMap = style->counterDirectives()) {
        CounterDirectives directives = directivesMap->get(counterName.impl());
        if (directives.m_reset) {
            value = directives.m_resetValue;
            if (directives.m_increment)
                value += directives.m_incrementValue;
            isReset = true;
            return true;
        }
        if (directives.m_increment) {
            value = directives.m_incrementValue;
            isReset = false;
            return true;
        }
    }

    if (counterName == "list-item") {
        if (object->isListItem()) {
            if (toRenderListItem(object)->hasExplicitValue()) {
                value = toRenderListItem(object)->explicitValue();
                isReset = true;
                return true;
            }
            value = 1;
            isReset = false;
            return true;
        }
        if (Node* e = object->node()) {
            if (e->hasTagName(olTag)) {
                value = static_cast<HTMLOListElement*>(e)->start();
                isReset = true;
                return true;
            }
            if (e->hasTagName(ulTag) || e->hasTagName(menuTag) || e->hasTagName(dirTag)) {
                value = 0;
                isReset = true;
                return true;
            }
        }
    }

    return false;
}

// - Finds the insertion point for the counter described by counterOwner, isReset and 
// identifier in the CounterNode tree for identifier and sets parent and
// previousSibling accordingly.
// - The function returns true if the counter whose insertion point is searched is NOT
// the root of the tree.
// - The root of the tree is a counter reference that is not in the scope of any other
// counter with the same identifier.
// - All the counter references with the same identifier as this one that are in
// children or subsequent siblings of the renderer that owns the root of the tree
// form the rest of of the nodes of the tree.
// - The root of the tree is always a reset type reference.
// - A subtree rooted at any reset node in the tree is equivalent to all counter 
// references that are in the scope of the counter or nested counter defined by that
// reset node.
// - Non-reset CounterNodes cannot have descendants.

static bool findPlaceForCounter(RenderObject* counterOwner, const AtomicString& identifier, bool isReset, CounterNode*& parent, CounterNode*& previousSibling)
{
    // We cannot stop searching for counters with the same identifier before we also
    // check this renderer, because it may affect the positioning in the tree of our counter.
    RenderObject* searchEndRenderer = previousSiblingOrParent(counterOwner);
    // We check renderers in preOrder from the renderer that our counter is attached to
    // towards the begining of the document for counters with the same identifier as the one
    // we are trying to find a place for. This is the next renderer to be checked.
    RenderObject* currentRenderer = counterOwner->previousInPreOrder();
    previousSibling = 0;
    while (currentRenderer) {
        CounterNode* currentCounter = makeCounterNode(currentRenderer, identifier, false);
        if (searchEndRenderer == currentRenderer) {
            // We may be at the end of our search.
            if (currentCounter) {
                // We have a suitable counter on the EndSearchRenderer.
                if (previousSibling) { // But we already found another counter that we come after.
                    if (currentCounter->isReset()) {
                        // We found a reset counter that is on a renderer that is a sibling of ours or a parent.
                        if (isReset && currentRenderer->parent() == counterOwner->parent()) {
                            // We are also a reset counter and the previous reset was on a sibling renderer
                            // hence we are the next sibling of that counter if that reset is not a root or
                            // we are a root node if that reset is a root.
                            parent = currentCounter->parent();
                            previousSibling = parent ? currentCounter : 0;
                            return parent;
                        }
                        // We are not a reset node or the previous reset must be on an ancestor of our renderer
                        // hence we must be a child of that reset counter.
                        parent = currentCounter;
                        ASSERT(previousSibling->parent() == currentCounter);
                        return true;
                    }
                    // CurrentCounter, the counter at the EndSearchRenderer, is not reset.
                    if (!isReset || currentRenderer->parent() != counterOwner->parent()) {
                        // If the node we are placing is not reset or we have found a counter that is attached
                        // to an ancestor of the placed counter's renderer we know we are a sibling of that node.
                        ASSERT(currentCounter->parent() == previousSibling->parent());
                        parent = currentCounter->parent();
                        return true;
                    }
                } else { 
                    // We are at the potential end of the search, but we had no previous sibling candidate
                    // In this case we follow pretty much the same logic as above but no ASSERTs about 
                    // previousSibling, and when we are a sibling of the end counter we must set previousSibling
                    // to currentCounter.
                    if (currentCounter->isReset()) {
                        if (isReset && currentRenderer->parent() == counterOwner->parent()) {
                            parent = currentCounter->parent();
                            previousSibling = currentCounter;
                            return parent;
                        }
                        parent = currentCounter;
                        return true;
                    }
                    if (!isReset || currentRenderer->parent() != counterOwner->parent()) {
                        parent = currentCounter->parent();
                        previousSibling = currentCounter;
                        return true;
                    }
                    previousSibling = currentCounter;
                }
            }
            // We come here if the previous sibling or parent of our renderer had no 
            // good counter, or we are a reset node and the counter on the previous sibling
            // of our renderer was not a reset counter.
            // Set a new goal for the end of the search.
            searchEndRenderer = previousSiblingOrParent(currentRenderer);
        } else {
            // We are searching descendants of a previous sibling of the renderer that the
            // counter being placed is attached to.
            if (currentCounter) {
                // We found a suitable counter.
                if (previousSibling) {
                    // Since we had a suitable previous counter before, we should only consider this one as our 
                    // previousSibling if it is a reset counter and hence the current previousSibling is its child.
                    if (currentCounter->isReset()) {
                        previousSibling = currentCounter;
                        // We are no longer interested in previous siblings of the currentRenderer or their children
                        // as counters they may have attached cannot be the previous sibling of the counter we are placing.
                        currentRenderer = currentRenderer->parent();
                        continue;
                    }
                } else
                    previousSibling = currentCounter;
                currentRenderer = previousSiblingOrParent(currentRenderer);
                continue;
            }
        }
        // This function is designed so that the same test is not done twice in an iteration, except for this one
        // which may be done twice in some cases. Rearranging the decision points though, to accommodate this 
        // performance improvement would create more code duplication than is worthwhile in my oppinion and may further
        // impede the readability of this already complex algorithm.
        if (previousSibling)
            currentRenderer = previousSiblingOrParent(currentRenderer);
        else
            currentRenderer = currentRenderer->previousInPreOrder();
    }
    return false;
}

static CounterNode* makeCounterNode(RenderObject* object, const AtomicString& counterName, bool alwaysCreateCounter)
{
    ASSERT(object);

    if (object->m_hasCounterNodeMap)
        if (CounterMap* nodeMap = counterMaps().get(object))
            if (CounterNode* node = nodeMap->get(counterName.impl()))
                return node;

    bool isReset = false;
    int value = 0;
    if (!planCounter(object, counterName, isReset, value) && !alwaysCreateCounter)
        return 0;

    CounterNode* newParent = 0;
    CounterNode* newPreviousSibling = 0;
    CounterNode* newNode;
    if (findPlaceForCounter(object, counterName, isReset, newParent, newPreviousSibling)) {
        newNode = new CounterNode(object, isReset, value);
        newParent->insertAfter(newNode, newPreviousSibling, counterName);
    } else {
        // Make a reset node for counters that aren't inside an existing reset node.
        newNode = new CounterNode(object, true, value);
    }

    CounterMap* nodeMap;
    if (object->m_hasCounterNodeMap)
        nodeMap = counterMaps().get(object);
    else {
        nodeMap = new CounterMap;
        counterMaps().set(object, nodeMap);
        object->m_hasCounterNodeMap = true;
    }
    nodeMap->set(counterName.impl(), newNode);

    return newNode;
}

RenderCounter::RenderCounter(Document* node, const CounterContent& counter)
    : RenderText(node, StringImpl::empty())
    , m_counter(counter)
    , m_counterNode(0)
{
}

const char* RenderCounter::renderName() const
{
    return "RenderCounter";
}

bool RenderCounter::isCounter() const
{
    return true;
}

PassRefPtr<StringImpl> RenderCounter::originalText() const
{
    if (!parent())
        return 0;

    if (!m_counterNode)
        m_counterNode = makeCounterNode(parent(), m_counter.identifier(), true);

    CounterNode* child = m_counterNode;
    int value = child->isReset() ? child->value() : child->countInParent();

    String text = listMarkerText(m_counter.listStyle(), value);

    if (!m_counter.separator().isNull()) {
        if (!child->isReset())
            child = child->parent();
        while (CounterNode* parent = child->parent()) {
            text = listMarkerText(m_counter.listStyle(), child->countInParent())
                + m_counter.separator() + text;
            child = parent;
        }
    }

    return text.impl();
}

void RenderCounter::calcPrefWidths(int lead)
{
    setTextInternal(originalText());
    RenderText::calcPrefWidths(lead);
}

void RenderCounter::invalidate(const AtomicString& identifier)
{
    if (m_counter.identifier() != identifier)
        return;
    m_counterNode = 0;
    setNeedsLayoutAndPrefWidthsRecalc();
}

static void destroyCounterNodeChildren(const AtomicString& identifier, CounterNode* node)
{
    CounterNode* previous;
    for (CounterNode* child = node->lastDescendant(); child && child != node; child = previous) {
        previous = child->previousInPreOrder();
        child->parent()->removeChild(child, identifier);
        ASSERT(counterMaps().get(child->renderer())->get(identifier.impl()) == child);
        counterMaps().get(child->renderer())->remove(identifier.impl());
        if (!child->renderer()->documentBeingDestroyed()) {
            RenderObjectChildList* children = child->renderer()->virtualChildren();
            if (children)
                children->invalidateCounters(child->renderer(), identifier);
        }
        delete child;
    }
}

void RenderCounter::destroyCounterNodes(RenderObject* object)
{
    CounterMaps& maps = counterMaps();
    CounterMap* map = maps.get(object);
    if (!map)
        return;
    maps.remove(object);

    CounterMap::const_iterator end = map->end();
    for (CounterMap::const_iterator it = map->begin(); it != end; ++it) {
        CounterNode* node = it->second;
        AtomicString identifier(it->first.get());
        destroyCounterNodeChildren(identifier, node);
        if (CounterNode* parent = node->parent())
            parent->removeChild(node, identifier);
        delete node;
    }

    delete map;
}

} // namespace WebCore
