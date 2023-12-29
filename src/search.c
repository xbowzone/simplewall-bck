// simplewall
// Copyright (c) 2012-2021 dmex
// Copyright (c) 2021-2023 Henry++

#include "global.h"

VOID _app_search_initializetheme (
	_Inout_ PSEARCH_CONTEXT context
)
{
	RECT rect;
	HBITMAP hbitmap;
	HICON hicon_prev;
	HTHEME htheme;
	LONG dpi_value;
	HRESULT status;

	GetWindowRect (context->hwnd, &rect);

	dpi_value = _r_dc_getmonitordpi (&rect);

	// initialize borders
	context->cx_width = _r_dc_getdpi (20, dpi_value);
	context->cx_border = 0;

	if (IsThemeActive ())
	{
		htheme = OpenThemeData (context->hwnd, VSCLASS_EDIT);

		if (htheme)
		{
			status = GetThemeInt (htheme, 0, 0, TMT_BORDERSIZE, &context->cx_border);

			if (FAILED (status))
				context->cx_border = 0;

			CloseThemeData (htheme);
		}
	}

	if (!context->cx_border)
		context->cx_border = _r_dc_getsystemmetrics (SM_CXBORDER, dpi_value) * 2;

	// initialize icons
	context->image_width = _r_dc_getsystemmetrics (SM_CXSMICON, dpi_value) + _r_dc_getdpi (4, dpi_value);
	context->image_height = _r_dc_getsystemmetrics (SM_CYSMICON, dpi_value) + _r_dc_getdpi (4, dpi_value);

	status = _r_res_loadimage (_r_sys_getimagebase (), L"PNG", MAKEINTRESOURCE (IDP_SEARCH), &GUID_ContainerFormatPng, context->image_width, context->image_height, &hbitmap);

	if (NT_SUCCESS (status))
	{
		hicon_prev = context->hicon;

		context->hicon = _r_dc_bitmaptoicon (hbitmap, context->image_width, context->image_height);

		if (context->hicon)
		{
			if (hicon_prev)
				DestroyIcon (hicon_prev);
		}

		DeleteObject (hbitmap);
	}
}

VOID _app_search_destroytheme (
	_Inout_ PSEARCH_CONTEXT context
)
{
	SAFE_DELETE_ICON (context->hicon);
}

VOID _app_search_initialize (
	_In_ HWND hwnd
)
{
	PSEARCH_CONTEXT context;
	WCHAR buffer[128];

	context = _r_mem_allocate (sizeof (SEARCH_CONTEXT));

	context->hwnd = hwnd;

	_app_search_initializetheme (context);

	_r_wnd_setcontext (context->hwnd, SHORT_MAX, context);

	context->def_window_proc = (WNDPROC)GetWindowLongPtrW (context->hwnd, GWLP_WNDPROC);
	SetWindowLongPtrW (context->hwnd, GWLP_WNDPROC, (LONG_PTR)_app_search_subclass_proc);

	_r_str_printf (buffer, RTL_NUMBER_OF (buffer), L"%s...", _r_locale_getstring (IDS_FIND));

	_r_edit_setcuebanner (context->hwnd, 0, buffer);

	SendMessageW (context->hwnd, WM_THEMECHANGED, 0, 0);
}

VOID _app_search_setvisible (
	_In_ HWND hwnd,
	_In_ HWND hsearch
)
{
	BOOLEAN is_visible;

	is_visible = _r_config_getboolean (L"IsShowSearchBar", TRUE);

	if (is_visible)
	{
		ShowWindow (hsearch, SW_SHOWNA);

		if (_r_wnd_isvisible (hwnd))
			SetFocus (hsearch);
	}
	else
	{
		_r_ctrl_setstring (hsearch, 0, L"");

		ShowWindow (hsearch, SW_HIDE);
	}
}

VOID _app_search_drawbutton (
	_Inout_ PSEARCH_CONTEXT context,
	_In_ LPCRECT button_rect
)
{
	HBITMAP buffer_bitmap;
	HBITMAP old_bitmap;
	HDC buffer_dc;
	HDC hdc;
	RECT rect;

	hdc = GetWindowDC (context->hwnd);

	if (!hdc)
		return;

	SetRect (&rect, 0, 0, button_rect->right - button_rect->left, button_rect->bottom - button_rect->top);

	buffer_dc = CreateCompatibleDC (hdc);
	buffer_bitmap = CreateCompatibleBitmap (hdc, rect.right, rect.bottom);

	old_bitmap = SelectObject (buffer_dc, buffer_bitmap);

	if (context->is_pushed)
	{
		_r_dc_fillrect (buffer_dc, &rect, GetSysColor (COLOR_BTNHILIGHT));
	}
	else if (context->is_hot)
	{
		_r_dc_fillrect (buffer_dc, &rect, GetSysColor (COLOR_HOTLIGHT));
	}
	else
	{
		_r_dc_fillrect (buffer_dc, &rect, GetSysColor (COLOR_WINDOW));
	}

	DrawIconEx (buffer_dc, rect.left + 1, rect.top, context->hicon, context->image_width, context->image_height, 0, NULL, DI_NORMAL);

	BitBlt (hdc, button_rect->left, button_rect->top, button_rect->right, button_rect->bottom, buffer_dc, 0, 0, SRCCOPY);

	SelectObject (buffer_dc, old_bitmap);
	DeleteObject (buffer_bitmap);
	DeleteDC (buffer_dc);

	ReleaseDC (context->hwnd, hdc);
}

VOID _app_search_getbuttonrect (
	_In_ PSEARCH_CONTEXT context,
	_Inout_ PRECT rect
)
{
	rect->left = (rect->right - context->cx_width) - context->cx_border - 1;
	rect->top += context->cx_border;
	rect->right -= context->cx_border;
	rect->bottom -= context->cx_border;
}

BOOLEAN _app_search_isstringfound (
	_In_opt_ PR_STRING string,
	_In_ PR_STRING search_string,
	_Inout_ PITEM_LISTVIEW_CONTEXT context,
	_Inout_ PBOOLEAN is_changed
)
{
	if (!string)
	{
		if (context->is_hidden)
		{
			context->is_hidden = FALSE;

			*is_changed = TRUE;
		}

		return FALSE;
	}

	if (_r_str_findstring (&string->sr, &search_string->sr, TRUE) != SIZE_MAX)
	{
		if (context->is_hidden)
		{
			context->is_hidden = FALSE;

			*is_changed = TRUE;
		}

		return TRUE;
	}
	else
	{
		if (!context->is_hidden)
		{
			context->is_hidden = TRUE;

			*is_changed = TRUE;
		}
	}

	return FALSE;
}

BOOLEAN _app_search_applyfiltercallback (
	_In_ HWND hwnd,
	_In_ INT listview_id,
	_In_opt_ PR_STRING search_string
)
{
	PITEM_LISTVIEW_CONTEXT context;
	INT item_count;
	BOOLEAN is_changed = FALSE;

	item_count = _r_listview_getitemcount (hwnd, listview_id);

	if (!item_count)
		return FALSE;

	for (INT i = 0; i < item_count; i++)
	{
		context = (PITEM_LISTVIEW_CONTEXT)_r_listview_getitemlparam (hwnd, listview_id, i);

		if (!context)
			continue;

		if (_app_search_applyfilteritem (hwnd, listview_id, i, context, search_string))
			is_changed = TRUE;
	}

	if (is_changed)
		_app_listview_updateby_id (hwnd, listview_id, PR_UPDATE_NOSETVIEW | PR_UPDATE_FORCE);

	return is_changed;
}

BOOLEAN _app_search_applyfilteritem (
	_In_ HWND hwnd,
	_In_ INT listview_id,
	_In_ INT item_id,
	_Inout_ PITEM_LISTVIEW_CONTEXT context,
	_In_opt_ PR_STRING search_string
)
{
	PITEM_APP ptr_app = NULL;
	PITEM_RULE ptr_rule = NULL;
	PITEM_NETWORK ptr_network = NULL;
	PITEM_LOG ptr_log = NULL;
	PR_STRING string;
	BOOLEAN is_changed = FALSE;

	// reset hidden state
	if (context->is_hidden)
	{
		context->is_hidden = FALSE;

		is_changed = TRUE;
	}

	if (!search_string)
		goto CleanupExit;

	switch (listview_id)
	{
		case IDC_APPS_PROFILE:
		case IDC_APPS_SERVICE:
		case IDC_APPS_UWP:
		case IDC_RULE_APPS_ID:
		{
			ptr_app = _app_getappitem (context->id_code);

			if (!ptr_app)
				goto CleanupExit;

			// path
			if (ptr_app->real_path)
			{
				if (_app_search_isstringfound (ptr_app->real_path, search_string, context, &is_changed))
					goto CleanupExit;
			}

			// comment
			if (!_r_obj_isstringempty (ptr_app->comment))
			{
				if (_app_search_isstringfound (ptr_app->comment, search_string, context, &is_changed))
					goto CleanupExit;
			}

			break;
		}

		case IDC_RULES_BLOCKLIST:
		case IDC_RULES_SYSTEM:
		case IDC_RULES_CUSTOM:
		case IDC_APP_RULES_ID:
		{
			ptr_rule = _app_getrulebyid (context->id_code);

			if (!ptr_rule)
				goto CleanupExit;

			if (ptr_rule->name)
			{
				if (_app_search_isstringfound (ptr_rule->name, search_string, context, &is_changed))
					goto CleanupExit;
			}

			if (ptr_rule->rule_remote)
			{
				if (_app_search_isstringfound (ptr_rule->rule_remote, search_string, context, &is_changed))
					goto CleanupExit;
			}

			if (ptr_rule->rule_local)
			{
				if (_app_search_isstringfound (ptr_rule->rule_local, search_string, context, &is_changed))
					goto CleanupExit;
			}

			if (ptr_rule->protocol_str)
			{
				if (_app_search_isstringfound (ptr_rule->protocol_str, search_string, context, &is_changed))
					goto CleanupExit;
			}

			// comment
			if (!_r_obj_isstringempty (ptr_rule->comment))
			{
				if (_app_search_isstringfound (ptr_rule->comment, search_string, context, &is_changed))
					goto CleanupExit;
			}

			break;
		}

		case IDC_NETWORK:
		{
			ptr_network = _app_network_getitem (context->id_code);

			if (!ptr_network)
				goto CleanupExit;

			// path
			if (ptr_network->path)
			{
				if (_app_search_isstringfound (ptr_network->path, search_string, context, &is_changed))
					goto CleanupExit;
			}

			// local address
			string = _InterlockedCompareExchangePointer (&ptr_network->local_addr_str, NULL, NULL);

			if (string)
			{
				if (_app_search_isstringfound (string, search_string, context, &is_changed))
					goto CleanupExit;
			}

			// local host
			string = _InterlockedCompareExchangePointer (&ptr_network->local_host_str, NULL, NULL);

			if (string)
			{
				if (_app_search_isstringfound (string, search_string, context, &is_changed))
					goto CleanupExit;
			}

			// remote address
			string = _InterlockedCompareExchangePointer (&ptr_network->remote_addr_str, NULL, NULL);

			if (string)
			{
				if (_app_search_isstringfound (string, search_string, context, &is_changed))
					goto CleanupExit;
			}

			// remote host
			string = _InterlockedCompareExchangePointer (&ptr_network->remote_host_str, NULL, NULL);

			if (string)
			{
				if (_app_search_isstringfound (string, search_string, context, &is_changed))
					goto CleanupExit;
			}

			// protocol
			if (ptr_network->protocol_str)
			{
				if (_app_search_isstringfound (ptr_network->protocol_str, search_string, context, &is_changed))
					goto CleanupExit;
			}

			break;
		}

		case IDC_LOG:
		{
			ptr_log = _app_getlogitem (context->id_code);

			if (!ptr_log)
				goto CleanupExit;

			// path
			if (ptr_log->path)
			{
				if (_app_search_isstringfound (ptr_log->path, search_string, context, &is_changed))
					goto CleanupExit;
			}

			// filter name
			if (ptr_log->filter_name)
			{
				if (_app_search_isstringfound (ptr_log->filter_name, search_string, context, &is_changed))
					goto CleanupExit;
			}

			// layer name
			if (ptr_log->layer_name)
			{
				if (_app_search_isstringfound (ptr_log->layer_name, search_string, context, &is_changed))
					goto CleanupExit;
			}

			// user name
			if (ptr_log->username)
			{
				if (_app_search_isstringfound (ptr_log->username, search_string, context, &is_changed))
					goto CleanupExit;
			}

			// local address
			string = _InterlockedCompareExchangePointer (&ptr_log->local_addr_str, NULL, NULL);

			if (string)
			{
				if (_app_search_isstringfound (string, search_string, context, &is_changed))
					goto CleanupExit;
			}

			// local host
			string = _InterlockedCompareExchangePointer (&ptr_log->local_host_str, NULL, NULL);

			if (string)
			{
				if (_app_search_isstringfound (string, search_string, context, &is_changed))
					goto CleanupExit;
			}

			// remote address
			string = _InterlockedCompareExchangePointer (&ptr_log->remote_addr_str, NULL, NULL);

			if (string)
			{
				if (_app_search_isstringfound (string, search_string, context, &is_changed))
					goto CleanupExit;
			}

			// remote host
			string = _InterlockedCompareExchangePointer (&ptr_log->remote_host_str, NULL, NULL);

			if (string)
			{
				if (_app_search_isstringfound (string, search_string, context, &is_changed))
					goto CleanupExit;
			}

			// protocol
			if (ptr_log->protocol_str)
			{
				if (_app_search_isstringfound (ptr_log->protocol_str, search_string, context, &is_changed))
					goto CleanupExit;
			}

			break;
		}
	}

CleanupExit:

	if (ptr_app)
		_r_obj_dereference (ptr_app);

	if (ptr_rule)
		_r_obj_dereference (ptr_rule);

	if (ptr_network)
		_r_obj_dereference (ptr_network);

	if (ptr_log)
		_r_obj_dereference (ptr_log);

	if (is_changed)
		_r_listview_setitem_ex (hwnd, listview_id, item_id, 0, NULL, I_IMAGECALLBACK, I_GROUPIDCALLBACK, 0);

	return is_changed;
}

VOID _app_search_applyfilter (
	_In_ HWND hwnd,
	_In_ INT listview_id,
	_In_opt_ PR_STRING search_string
)
{
	if (!((listview_id >= IDC_APPS_PROFILE && listview_id <= IDC_LOG) || listview_id == IDC_RULE_APPS_ID || listview_id == IDC_APP_RULES_ID))
		return;

	_app_search_applyfiltercallback (hwnd, listview_id, search_string);
}

LRESULT CALLBACK _app_search_subclass_proc (
	_In_ HWND hwnd,
	_In_ UINT msg,
	_In_ WPARAM wparam,
	_In_ LPARAM lparam
)
{
	PSEARCH_CONTEXT context;
	WNDPROC old_wnd_proc;

	context = (PSEARCH_CONTEXT)_r_wnd_getcontext (hwnd, SHORT_MAX);

	if (!context)
		return FALSE;

	old_wnd_proc = context->def_window_proc;

	switch (msg)
	{
		case WM_NCDESTROY:
		{
			_app_search_destroytheme (context);

			_r_wnd_removecontext (context->hwnd, SHORT_MAX);
			SetWindowLongPtrW (hwnd, GWLP_WNDPROC, (LONG_PTR)old_wnd_proc);

			_r_mem_free (context);
			context = NULL;

			break;
		}

		case WM_ERASEBKGND:
		{
			return TRUE;
		}

		case WM_NCCALCSIZE:
		{
			LPNCCALCSIZE_PARAMS calc_size;

			calc_size = (LPNCCALCSIZE_PARAMS)lparam;

			// Let Windows handle the non-client defaults.
			CallWindowProcW (old_wnd_proc, hwnd, msg, wparam, lparam);

			// Deflate the client area to accommodate the custom button.
			calc_size->rgrc[0].right -= context->cx_width;

			return FALSE;
		}

		case WM_NCPAINT:
		{
			RECT rect;

			// Let Windows handle the non-client defaults.
			CallWindowProcW (old_wnd_proc, hwnd, msg, wparam, lparam);

			// Get the screen coordinates of the window.
			if (!GetWindowRect (hwnd, &rect))
				return FALSE;

			// Adjust the coordinates (start from 0,0).
			OffsetRect (&rect, -rect.left, -rect.top);

			// Get the position of the inserted button.
			_app_search_getbuttonrect (context, &rect);

			// Draw the button.
			_app_search_drawbutton (context, &rect);

			return FALSE;
		}

		case WM_NCHITTEST:
		{
			POINT point;
			RECT rect;

			// Get the screen coordinates of the mouse.
			if (!GetCursorPos (&point))
				break;

			// Get the screen coordinates of the window.
			if (!GetWindowRect (hwnd, &rect))
				break;

			// Get the position of the inserted button.
			_app_search_getbuttonrect (context, &rect);

			// Check that the mouse is within the inserted button.
			if (PtInRect (&rect, point))
				return HTBORDER;

			break;
		}

		case WM_NCLBUTTONDOWN:
		{
			POINT point;
			RECT rect;

			// Get the screen coordinates of the mouse.
			if (!GetCursorPos (&point))
				break;

			// Get the screen coordinates of the window.
			if (!GetWindowRect (hwnd, &rect))
				break;

			// Get the position of the inserted button.
			_app_search_getbuttonrect (context, &rect);

			// Check that the mouse is within the inserted button.
			if (!PtInRect (&rect, point))
				break;

			context->is_pushed = TRUE;

			SetCapture (hwnd);

			RedrawWindow (hwnd, NULL, NULL, RDW_FRAME | RDW_INVALIDATE);

			break;
		}

		case WM_LBUTTONUP:
		{
			POINT point;
			RECT rect;

			// Get the screen coordinates of the mouse.
			if (!GetCursorPos (&point))
				break;

			// Get the screen coordinates of the window.
			if (!GetWindowRect (hwnd, &rect))
				break;

			// Get the position of the inserted button.
			_app_search_getbuttonrect (context, &rect);

			// Check that the mouse is within the inserted button.
			if (PtInRect (&rect, point))
			{
				SetFocus (hwnd);

				_r_ctrl_setstring (hwnd, 0, L"");
			}

			if (GetCapture () == hwnd)
			{
				context->is_pushed = FALSE;

				ReleaseCapture ();
			}

			RedrawWindow (hwnd, NULL, NULL, RDW_FRAME | RDW_INVALIDATE);

			break;
		}

		case WM_CUT:
		case WM_CLEAR:
		case WM_PASTE:
		case WM_UNDO:
		case WM_KEYUP:
		case WM_SETTEXT:
		case WM_KILLFOCUS:
		{
			RedrawWindow (hwnd, NULL, NULL, RDW_FRAME | RDW_INVALIDATE);
			break;
		}

		case WM_SETTINGCHANGE:
		case WM_SYSCOLORCHANGE:
		case WM_THEMECHANGED:
		case WM_DPICHANGED_AFTERPARENT:
		{
			_app_search_destroytheme (context);
			_app_search_initializetheme (context);

			// Reset the client area margins.
			_r_ctrl_settextmargin (hwnd, 0, 0, 0);

			// Refresh the non-client area.
			SetWindowPos (hwnd, NULL, 0, 0, 0, 0, SWP_NOACTIVATE | SWP_NOMOVE | SWP_NOSIZE | SWP_FRAMECHANGED);

			// Force the edit control to update its non-client area.
			RedrawWindow (hwnd, NULL, NULL, RDW_FRAME | RDW_INVALIDATE);

			break;
		}

		case WM_MOUSEMOVE:
		case WM_NCMOUSEMOVE:
		{
			TRACKMOUSEEVENT tme = {0};
			POINT point;
			RECT rect;

			// Get the screen coordinates of the mouse.
			if (!GetCursorPos (&point))
				break;

			// Get the screen coordinates of the window.
			if (!GetWindowRect (hwnd, &rect))
				break;

			// Get the position of the inserted button.
			_app_search_getbuttonrect (context, &rect);

			// Check that the mouse is within the inserted button.
			if ((wparam & MK_LBUTTON) && GetCapture () == hwnd)
				context->is_pushed = PtInRect (&rect, point);

			// Check that the mouse is within the inserted button.
			if (!context->is_hot)
			{
				tme.cbSize = sizeof (tme);
				tme.dwFlags = TME_LEAVE | TME_NONCLIENT;
				tme.hwndTrack = hwnd;
				tme.dwHoverTime = 0;

				context->is_hot = TRUE;

				TrackMouseEvent (&tme);
			}

			RedrawWindow (hwnd, NULL, NULL, RDW_FRAME | RDW_INVALIDATE);

			break;
		}

		case WM_MOUSELEAVE:
		case WM_NCMOUSELEAVE:
		{
			context->is_hot = FALSE;

			RedrawWindow (hwnd, NULL, NULL, RDW_FRAME | RDW_INVALIDATE);

			break;
		}
	}

	return CallWindowProcW (old_wnd_proc, hwnd, msg, wparam, lparam);
}
