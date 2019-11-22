//    Copyright 2019 namazso <admin@namazso.eu>
//    This file is part of OpenHashTab.
//
//    OpenHashTab is free software: you can redistribute it and/or modify
//    it under the terms of the GNU General Public License as published by
//    the Free Software Foundation, either version 3 of the License, or
//    (at your option) any later version.
//
//    OpenHashTab is distributed in the hope that it will be useful,
//    but WITHOUT ANY WARRANTY; without even the implied warranty of
//    MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
//    GNU General Public License for more details.
//
//    You should have received a copy of the GNU General Public License
//    along with OpenHashTab.  If not, see <https://www.gnu.org/licenses/>.
#include "stdafx.h"

#include "OpenHashTabShlExt.h"
#include "OpenHashTabPropPage.h"
#include "dllmain.h"
#include "utl.h"

// COpenHashTabShlExt

HRESULT STDMETHODCALLTYPE COpenHashTabShlExt::Initialize(
  _In_opt_  PCIDLIST_ABSOLUTE folder,
  _In_opt_  IDataObject*      data,
  _In_opt_  HKEY              prog_id
)
{
  UNREFERENCED_PARAMETER(folder);
  UNREFERENCED_PARAMETER(prog_id);

  // Init the common controls.
  INITCOMMONCONTROLSEX iccex
  {
    sizeof(INITCOMMONCONTROLSEX),
    ICC_WIN95_CLASSES
  };
  InitCommonControlsEx(&iccex);

  // Read the list of folders from the data object.  They're stored in HDROP
  // form, so just get the HDROP handle and then use the drag 'n' drop APIs
  // on it.
  FORMATETC etc
  {
    CF_HDROP,
    nullptr,
    DVASPECT_CONTENT,
    -1,
    TYMED_HGLOBAL
  };
  STGMEDIUM stg;
  if (FAILED(data->GetData(&etc, &stg)))
    return E_INVALIDARG;

  // Get an HDROP handle.
  const auto drop = (HDROP)GlobalLock(stg.hGlobal);

  if (!drop)
  {
    ReleaseStgMedium(&stg);
    return E_INVALIDARG;
  }

  // Determine how many files are involved in this operation.
  const auto file_count = DragQueryFile(
    drop,
    0xFFFFFFFF,
    nullptr,
    0
  );

  for (auto i = 0u; i < file_count; i++)
  {
    // NT paths can be 32k long, plus one for null terminator
    TCHAR file_name[0x8001];

    // Get the next filename.
    if (0 == DragQueryFile(drop, i, file_name, (UINT)std::size(file_name)))
      continue;

    // Skip directories. TODO: Maybe a setting entry for recursing into them?
    if (PathIsDirectory(file_name))
      continue;

    // Add the filename to our list of files to act on.
    _files.emplace_back(file_name);
  }

  // Release resources.
  GlobalUnlock(stg.hGlobal);
  ReleaseStgMedium(&stg);

  // If we found any files we can work with, return S_OK.  Otherwise,
  // return E_FAIL so we don't get called again for this right-click
  // operation.
  return _files.empty() ? E_FAIL : S_OK;
}

// IShellPropSheetExt

HRESULT STDMETHODCALLTYPE COpenHashTabShlExt::AddPages(
  _In_  LPFNSVADDPROPSHEETPAGE  add_page_proc,
  _In_  LPARAM                  lparam
)
{
  // We shouldn't ever get called with empty files, since Initialize should
  // return failure. So if we somehow do, just don't add any pages.
  assert(!_files.empty());
  if (_files.empty())
    return S_OK;

  const auto tab_name = utl::GetString(IDS_HASHES);

  // Set up everything but pfnDlgProc, pfnCallback, lParam which will be set
  // up by MakePropPageWrapper to call members functions on the page object
  PROPSHEETPAGE psp{};
  psp.dwSize = sizeof(PROPSHEETPAGE);
  psp.dwFlags = PSP_USEREFPARENT | PSP_USETITLE | PSP_USECALLBACK;
  psp.hInstance = _AtlBaseModule.GetResourceInstance();
  psp.pszTemplate = MAKEINTRESOURCE(IDD_OPENHASHTAB_PROPPAGE);
  psp.pszTitle = tab_name.c_str();
  psp.pcRefParent = (UINT*)&_AtlModule.m_nLockCnt;

  const auto page = new OpenHashTabPropPage(_files);
  const auto hpage = utl::MakePropPageWrapper(psp, page);

  if (hpage)
  {
    // Call the shell's callback function, so it adds the page to
    // the property sheet.
    if (!add_page_proc(hpage, lparam))
      DestroyPropertySheetPage(hpage);
  }

  return S_OK;
}

HRESULT STDMETHODCALLTYPE COpenHashTabShlExt::ReplacePage(
  _In_  EXPPS                   page_id,
  _In_  LPFNSVADDPROPSHEETPAGE  replace_with_proc,
  _In_  LPARAM                  lparam
)
{
  UNREFERENCED_PARAMETER(page_id);
  UNREFERENCED_PARAMETER(replace_with_proc);
  UNREFERENCED_PARAMETER(lparam);

  return E_NOTIMPL;
}
