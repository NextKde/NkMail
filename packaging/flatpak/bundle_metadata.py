# SPDX-License-Identifier: GPL-3.0-only
# Adapted from NextKde/kos-apps packaging/flatpak/prepare-bundle.py
# Source: e5bab138f8cbfab697b5d67c1f4a1cc653fb29b4 (2026-11-09).
import ctypes
import ctypes.util
import os


def bundle_metadata_and_size(path):
    """Read exact metadata bytes and installed size through the consumer API."""
    flatpak_name = ctypes.util.find_library("flatpak")
    gio_name = ctypes.util.find_library("gio-2.0")
    gobject_name = ctypes.util.find_library("gobject-2.0")
    glib_name = ctypes.util.find_library("glib-2.0")
    if not all((flatpak_name, gio_name, gobject_name, glib_name)):
        raise ValueError("Flatpak and GLib libraries are required to read bundle installed size")
    flatpak = ctypes.CDLL(flatpak_name)
    gio = ctypes.CDLL(gio_name)
    gobject = ctypes.CDLL(gobject_name)
    glib = ctypes.CDLL(glib_name)
    gio.g_file_new_for_path.argtypes = [ctypes.c_char_p]
    gio.g_file_new_for_path.restype = ctypes.c_void_p
    flatpak.flatpak_bundle_ref_new.argtypes = [ctypes.c_void_p, ctypes.POINTER(ctypes.c_void_p)]
    flatpak.flatpak_bundle_ref_new.restype = ctypes.c_void_p
    flatpak.flatpak_bundle_ref_get_installed_size.argtypes = [ctypes.c_void_p]
    flatpak.flatpak_bundle_ref_get_installed_size.restype = ctypes.c_uint64
    flatpak.flatpak_bundle_ref_get_metadata.argtypes = [ctypes.c_void_p]
    flatpak.flatpak_bundle_ref_get_metadata.restype = ctypes.c_void_p
    glib.g_bytes_get_data.argtypes = [ctypes.c_void_p, ctypes.POINTER(ctypes.c_size_t)]
    glib.g_bytes_get_data.restype = ctypes.c_void_p
    glib.g_bytes_unref.argtypes = [ctypes.c_void_p]
    glib.g_bytes_unref.restype = None
    gobject.g_object_unref.argtypes = [ctypes.c_void_p]
    gobject.g_object_unref.restype = None
    glib.g_error_free.argtypes = [ctypes.c_void_p]
    glib.g_error_free.restype = None
    file = gio.g_file_new_for_path(os.fsencode(path))
    if not file:
        raise ValueError("Flatpak could not open the exported bundle")
    error = ctypes.c_void_p()
    bundle = flatpak.flatpak_bundle_ref_new(file, ctypes.byref(error))
    metadata_bytes = None
    try:
        if not bundle:
            raise ValueError("Flatpak could not inspect the exported bundle")
        metadata_bytes = flatpak.flatpak_bundle_ref_get_metadata(bundle)
        if not metadata_bytes:
            raise ValueError("Exported bundle has no application metadata")
        length = ctypes.c_size_t()
        data = glib.g_bytes_get_data(metadata_bytes, ctypes.byref(length))
        if not data or not 0 < length.value <= 256 * 1024:
            raise ValueError("Exported bundle metadata is empty or exceeds the signed plan limit")
        metadata = ctypes.string_at(data, length.value)
        return metadata, int(flatpak.flatpak_bundle_ref_get_installed_size(bundle))
    finally:
        if metadata_bytes:
            glib.g_bytes_unref(metadata_bytes)
        if bundle:
            gobject.g_object_unref(bundle)
        if file:
            gobject.g_object_unref(file)
        if error:
            glib.g_error_free(error)
