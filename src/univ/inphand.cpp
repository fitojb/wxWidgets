///////////////////////////////////////////////////////////////////////////////
// Name:        univ/inphand.cpp
// Purpose:     (trivial) wxInputHandler implementation
// Author:      Vadim Zeitlin
// Modified by:
// Created:     18.08.00
// RCS-ID:      $Id$
// Copyright:   (c) 2000 Vadim Zeitlin <zeitlin@dptmaths.ens-cachan.fr>
// Licence:     wxWindows license
///////////////////////////////////////////////////////////////////////////////

// ===========================================================================
// declarations
// ===========================================================================

// ---------------------------------------------------------------------------
// headers
// ---------------------------------------------------------------------------

#ifdef __GNUG__
    #pragma implementation "inphand.h"
#endif

// For compilers that support precompilation, includes "wx.h".
#include "wx/wxprec.h"

#ifdef __BORLANDC__
    #pragma hdrstop
#endif

#ifndef WX_PRECOMP
#endif // WX_PRECOMP

#include "wx/univ/inphand.h"

// ============================================================================
// implementation
// ============================================================================

// ----------------------------------------------------------------------------
// wxInputHandler
// ----------------------------------------------------------------------------

bool wxInputHandler::OnMouseMove(wxControl * WXUNUSED(control),
                                 const wxMouseEvent& WXUNUSED(event))
{
    return FALSE;
}

wxInputHandler::~wxInputHandler()
{
}

// ----------------------------------------------------------------------------
// wxStdButtonInputHandler
// ----------------------------------------------------------------------------

wxStdButtonInputHandler::wxStdButtonInputHandler(wxInputHandler *handler)
                       : wxStdInputHandler(handler)
{
    m_winCapture = NULL;
}

wxControlActions wxStdButtonInputHandler::Map(wxControl *control,
                                              const wxKeyEvent& event,
                                              bool pressed)
{
    int keycode = event.GetKeyCode();
    if ( keycode == WXK_SPACE || keycode == WXK_RETURN )
    {
        return wxACTION_BUTTON_TOGGLE;
    }

    return wxStdInputHandler::Map(control, event, pressed);
}

wxControlActions wxStdButtonInputHandler::Map(wxControl *control,
                                              const wxMouseEvent& event)
{
    // the button has 2 states: pressed and normal with the following
    // transitions between them:
    //
    //      normal -> left down -> capture mouse and go to pressed state
    //      pressed -> left up inside -> generate click -> go to normal
    //                         outside ------------------>
    //
    // the other mouse buttons are ignored
    if ( event.Button(1) )
    {
        if ( event.ButtonDown(1) )
        {
            m_winCapture = control;
            m_winCapture->CaptureMouse();
            m_winHasMouse = TRUE;

            return wxACTION_BUTTON_PRESS;
        }
        else // up
        {
            m_winCapture->ReleaseMouse();
            m_winCapture = NULL;

            if ( m_winHasMouse )
            {
                // this will generate a click event
                return wxACTION_BUTTON_TOGGLE;
            }
            //else: the mouse was released outside the window, this doesn't
            //      count as a click
        }
    }

    return wxStdInputHandler::Map(control, event);
}

bool wxStdButtonInputHandler::OnMouseMove(wxControl *control,
                                          const wxMouseEvent& event)
{
    // we only have to do soemthing when the mouse leaves/enters the pressed
    // button and don't care about the other ones
    if ( event.GetEventObject() == m_winCapture )
    {
        // leaving the button should remove its pressed state
        if ( event.Leaving() )
        {
            // remember that the mouse is now outside
            m_winHasMouse = FALSE;

            // we do have a pressed button, so release it
            if ( control->PerformAction(wxACTION_BUTTON_RELEASE, event) )
            {
                // the button state changed, refresh needed
                return TRUE;
            }
        }
        // and entering it back should make it pressed again if it had been
        // pressed
        else if ( event.Entering() )
        {
            // the mouse is (back) inside the button
            m_winHasMouse = TRUE;

            // we did have a pressed button which we released when leaving the
            // window, press it again
            if ( control->PerformAction(wxACTION_BUTTON_PRESS, event) )
            {
                return TRUE;
            }
        }
    }

    return wxStdInputHandler::OnMouseMove(control, event);
}

// ----------------------------------------------------------------------------
// wxStdScrollBarInputHandler
// ----------------------------------------------------------------------------

wxStdScrollBarInputHandler::wxStdScrollBarInputHandler(wxRenderer *renderer,
                                                       wxInputHandler *handler)
                          : wxStdInputHandler(handler)
{
    m_renderer = renderer;
    m_winCapture = NULL;
    m_htLast = wxHT_NOWHERE;
}

void wxStdScrollBarInputHandler::SetElementState(wxScrollBar *control,
                                                 int flag,
                                                 bool doIt)
{
    if ( m_htLast > wxHT_SCROLLBAR_FIRST && m_htLast < wxHT_SCROLLBAR_LAST )
    {
        wxScrollBar::Element
            elem = (wxScrollBar::Element)(m_htLast - wxHT_SCROLLBAR_FIRST - 1);

        int flags = control->GetState(elem);
        if ( doIt )
            flags |= flag;
        else
            flags &= ~flag;
        control->SetState(elem, flags);
    }
}

wxControlActions wxStdScrollBarInputHandler::Map(wxControl *control,
                                                 const wxKeyEvent& event,
                                                 bool pressed)
{
    // we only react to the key presses here
    if ( pressed )
    {
        switch ( event.GetKeyCode() )
        {
            case WXK_DOWN:
            case WXK_RIGHT:     return wxACTION_SCROLL_LINE_DOWN;
            case WXK_UP:
            case WXK_LEFT:      return wxACTION_SCROLL_LINE_UP;
            case WXK_HOME:      return wxACTION_SCROLL_START;
            case WXK_END:       return wxACTION_SCROLL_END;
            case WXK_PRIOR:     return wxACTION_SCROLL_PAGE_UP;
            case WXK_NEXT:      return wxACTION_SCROLL_PAGE_DOWN;
        }
    }

    return wxStdInputHandler::Map(control, event, pressed);
}

wxControlActions wxStdScrollBarInputHandler::Map(wxControl *control,
                                                 const wxMouseEvent& event)
{
    // is this a click event from an acceptable button?
    int btn = -1;
    if ( event.IsButton() )
    {
        for ( int i = 1; i <= 3; i++ )
        {
            if ( event.Button(i) )
            {
                btn = i;
                break;
            }
        }

        wxASSERT_MSG( btn != -1, _T("unknown mouse button") );
    }

    if ( (btn != -1) && IsAllowedButton(btn) )
    {
        // determine which part of the window mouse is in
        wxScrollBar *scrollbar = wxStaticCast(control, wxScrollBar);
        wxHitTest ht = m_renderer->HitTestScrollbar
                                   (
                                    scrollbar,
                                    event.GetPosition()
                                   );

        // when the mouse is pressed on any scrollbar element, we capture it
        // and hold capture until the same mouse button is released
        if ( event.ButtonDown() )
        {
            if ( !m_winCapture )
            {
                m_btnCapture = btn;
                m_winCapture = control;
                m_winCapture->CaptureMouse();

                // generate the command
                bool hasAction = TRUE;
                wxControlAction action;
                switch ( ht )
                {
                    case wxHT_SCROLLBAR_ARROW_LINE_1:
                        action = wxACTION_SCROLL_LINE_UP;
                        break;

                    case wxHT_SCROLLBAR_ARROW_LINE_2:
                        action = wxACTION_SCROLL_LINE_DOWN;
                        break;

                    case wxHT_SCROLLBAR_BAR_1:
                        action = wxACTION_SCROLL_PAGE_UP;
                        break;

                    case wxHT_SCROLLBAR_BAR_2:
                        action = wxACTION_SCROLL_PAGE_DOWN;
                        break;

                    default:
                        hasAction = FALSE;
                }

                if ( hasAction )
                {
                    control->PerformAction(action, event);
                }

                // remove highlighting and press the arrow instead
                Highlight(scrollbar, FALSE);
                m_htLast = ht;
                Press(scrollbar, TRUE);

                control->Refresh();
            }
            //else: mouse already captured, nothing to do
        }
        // release mouse if the *same* button went up
        else if ( btn == m_btnCapture )
        {
            if ( m_winCapture )
            {
                m_winCapture->ReleaseMouse();
                m_winCapture = NULL;
                m_btnCapture = -1;

                // unpress the arrow and highlight the current element
                Press(scrollbar, FALSE);
                m_htLast = ht;
                Highlight(scrollbar, TRUE);

                control->Refresh();
            }
            else
            {
                // this is not supposed to happen as the button can't go up
                // without going down previously and then we'd have
                // m_winCapture by now
                wxFAIL_MSG( _T("logic error in mouse capturing code") );
            }
        }
    }

    return wxStdInputHandler::Map(control, event);
}

bool wxStdScrollBarInputHandler::OnMouseMove(wxControl *control,
                                             const wxMouseEvent& event)
{
    if ( m_winCapture )
    {
        // everything is locked while the mouse is captured, so don't do
        // anything
        return FALSE;
    }

    wxScrollBar *scrollbar = wxStaticCast(control, wxScrollBar);

    if ( event.Moving() )
    {
        wxHitTest ht = m_renderer->HitTestScrollbar
                                   (
                                    scrollbar,
                                    event.GetPosition()
                                   );
        if ( ht == m_htLast )
        {
            // nothing changed
            return FALSE;
        }

#ifdef DEBUG_MOUSE
        wxLogDebug("Scrollbar::OnMouseMove: ht = %d", ht);
#endif // DEBUG_MOUSE

        Highlight(scrollbar, FALSE);
        m_htLast = ht;
        Highlight(scrollbar, TRUE);
    }
    else if ( event.Leaving() )
    {
        Highlight(scrollbar, FALSE);
        m_htLast = wxHT_NOWHERE;
    }

    // highlighting changed
    return TRUE;
}
