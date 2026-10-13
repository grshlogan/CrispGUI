"""Drive an X11 window found by _NET_WM_PID with ctypes + libX11: shot <pid> <png> | click <pid> <x> <y> | close <pid> | resize <pid> <w> <h>."""
import ctypes, ctypes.util, struct, sys, time, zlib

x11 = ctypes.CDLL(ctypes.util.find_library("X11"))
c_ulong_p = ctypes.POINTER(ctypes.c_ulong)
x11.XOpenDisplay.restype = ctypes.c_void_p
x11.XDefaultRootWindow.restype = ctypes.c_ulong
x11.XDefaultRootWindow.argtypes = [ctypes.c_void_p]
x11.XInternAtom.restype = ctypes.c_ulong
x11.XInternAtom.argtypes = [ctypes.c_void_p, ctypes.c_char_p, ctypes.c_int]
x11.XQueryTree.argtypes = [ctypes.c_void_p, ctypes.c_ulong, c_ulong_p, c_ulong_p, ctypes.POINTER(c_ulong_p), ctypes.POINTER(ctypes.c_uint)]
x11.XGetWindowProperty.argtypes = [ctypes.c_void_p, ctypes.c_ulong, ctypes.c_ulong, ctypes.c_long, ctypes.c_long, ctypes.c_int, ctypes.c_ulong,
                                   c_ulong_p, ctypes.POINTER(ctypes.c_int), c_ulong_p, c_ulong_p, ctypes.POINTER(ctypes.POINTER(ctypes.c_ubyte))]
x11.XSendEvent.argtypes = [ctypes.c_void_p, ctypes.c_ulong, ctypes.c_int, ctypes.c_long, ctypes.c_void_p]
x11.XFlush.argtypes = [ctypes.c_void_p]

class XWindowAttributes(ctypes.Structure):
    _fields_ = [("x", ctypes.c_int), ("y", ctypes.c_int), ("width", ctypes.c_int), ("height", ctypes.c_int),
                ("border_width", ctypes.c_int), ("depth", ctypes.c_int), ("visual", ctypes.c_void_p), ("root", ctypes.c_ulong),
                ("class_", ctypes.c_int), ("bit_gravity", ctypes.c_int), ("win_gravity", ctypes.c_int), ("backing_store", ctypes.c_int),
                ("backing_planes", ctypes.c_ulong), ("backing_pixel", ctypes.c_ulong), ("save_under", ctypes.c_int),
                ("colormap", ctypes.c_ulong), ("map_installed", ctypes.c_int), ("map_state", ctypes.c_int),
                ("all_event_masks", ctypes.c_long), ("your_event_mask", ctypes.c_long), ("do_not_propagate_mask", ctypes.c_long),
                ("override_redirect", ctypes.c_int), ("screen", ctypes.c_void_p)]

class XImage(ctypes.Structure):
    _fields_ = [("width", ctypes.c_int), ("height", ctypes.c_int), ("xoffset", ctypes.c_int), ("format", ctypes.c_int),
                ("data", ctypes.POINTER(ctypes.c_ubyte)), ("byte_order", ctypes.c_int), ("bitmap_unit", ctypes.c_int),
                ("bitmap_bit_order", ctypes.c_int), ("bitmap_pad", ctypes.c_int), ("depth", ctypes.c_int),
                ("bytes_per_line", ctypes.c_int), ("bits_per_pixel", ctypes.c_int)]

class XButtonEvent(ctypes.Structure):  # also used for MotionNotify (is_hint shares the button slot's first byte)
    _fields_ = [("type", ctypes.c_int), ("serial", ctypes.c_ulong), ("send_event", ctypes.c_int), ("display", ctypes.c_void_p),
                ("window", ctypes.c_ulong), ("root", ctypes.c_ulong), ("subwindow", ctypes.c_ulong), ("time", ctypes.c_ulong),
                ("x", ctypes.c_int), ("y", ctypes.c_int), ("x_root", ctypes.c_int), ("y_root", ctypes.c_int),
                ("state", ctypes.c_uint), ("button", ctypes.c_uint), ("same_screen", ctypes.c_int)]

class XClientMessageEvent(ctypes.Structure):
    _fields_ = [("type", ctypes.c_int), ("serial", ctypes.c_ulong), ("send_event", ctypes.c_int), ("display", ctypes.c_void_p),
                ("window", ctypes.c_ulong), ("message_type", ctypes.c_ulong), ("format", ctypes.c_int), ("data", ctypes.c_long * 5)]

class XEvent(ctypes.Union):
    _fields_ = [("button", XButtonEvent), ("client", XClientMessageEvent), ("pad", ctypes.c_long * 24)]

x11.XGetWindowAttributes.argtypes = [ctypes.c_void_p, ctypes.c_ulong, ctypes.POINTER(XWindowAttributes)]
x11.XGetImage.restype = ctypes.POINTER(XImage)
x11.XGetImage.argtypes = [ctypes.c_void_p, ctypes.c_ulong, ctypes.c_int, ctypes.c_int, ctypes.c_uint, ctypes.c_uint, ctypes.c_ulong, ctypes.c_int]

def window_pid(dpy, win):
    atom = x11.XInternAtom(dpy, b"_NET_WM_PID", 1)
    t, f, n, rest, data = ctypes.c_ulong(), ctypes.c_int(), ctypes.c_ulong(), ctypes.c_ulong(), ctypes.POINTER(ctypes.c_ubyte)()
    if not atom or x11.XGetWindowProperty(dpy, win, atom, 0, 1, 0, 0, ctypes.byref(t), ctypes.byref(f), ctypes.byref(n),
                                          ctypes.byref(rest), ctypes.byref(data)) != 0 or n.value == 0:
        return None
    return ctypes.cast(data, c_ulong_p)[0]

def find(dpy, win, pid):
    if window_pid(dpy, win) == pid:
        return win
    root, parent, children, n = ctypes.c_ulong(), ctypes.c_ulong(), c_ulong_p(), ctypes.c_uint()
    if x11.XQueryTree(dpy, win, ctypes.byref(root), ctypes.byref(parent), ctypes.byref(children), ctypes.byref(n)):
        for i in range(n.value):
            hit = find(dpy, children[i], pid)
            if hit:
                return hit
    return None

def shot(dpy, win, path):
    attrs = XWindowAttributes()
    x11.XGetWindowAttributes(dpy, win, ctypes.byref(attrs))
    img = x11.XGetImage(dpy, win, 0, 0, attrs.width, attrs.height, 0xFFFFFFFF, 2).contents
    rows = []
    for y in range(img.height):
        base, row = y * img.bytes_per_line, bytearray(b"\x00")
        for x in range(img.width):
            row += bytes((img.data[base + 4 * x + 2], img.data[base + 4 * x + 1], img.data[base + 4 * x]))
        rows.append(bytes(row))
    chunk = lambda tag, data: struct.pack(">I", len(data)) + tag + data + struct.pack(">I", zlib.crc32(tag + data) & 0xFFFFFFFF)
    open(path, "wb").write(b"\x89PNG\r\n\x1a\n" + chunk(b"IHDR", struct.pack(">IIBBBBB", img.width, img.height, 8, 2, 0, 0, 0))
                           + chunk(b"IDAT", zlib.compress(b"".join(rows), 6)) + chunk(b"IEND", b""))
    print(f"saved {path} {img.width}x{img.height}")

def pointer_event(dpy, win, kind, x, y, button=1, state=0):
    ev = XEvent()
    ev.button.type, ev.button.display, ev.button.window = kind, dpy, win
    ev.button.root, ev.button.x, ev.button.y, ev.button.x_root, ev.button.y_root = x11.XDefaultRootWindow(dpy), x, y, x, y
    ev.button.same_screen, ev.button.button, ev.button.state = 1, (0 if kind == 6 else button), state
    mask = {4: 1 << 2, 5: 1 << 3, 6: 1 << 6}[kind]  # ButtonPressMask, ButtonReleaseMask, PointerMotionMask
    x11.XSendEvent(dpy, win, 1, mask, ctypes.byref(ev))
    x11.XFlush(dpy)

def click(dpy, win, x, y):
    for _ in range(3):
        pointer_event(dpy, win, 6, x, y); time.sleep(0.05)   # MotionNotify
    pointer_event(dpy, win, 4, x, y); time.sleep(0.08)       # ButtonPress
    pointer_event(dpy, win, 5, x, y, state=1 << 8)           # ButtonRelease with Button1Mask
    print(f"clicked {x},{y}")

def close(dpy, win):
    ev = XEvent()
    ev.client.type, ev.client.display, ev.client.window = 33, dpy, win  # ClientMessage
    ev.client.message_type, ev.client.format = x11.XInternAtom(dpy, b"WM_PROTOCOLS", 0), 32
    ev.client.data[0], ev.client.data[1] = x11.XInternAtom(dpy, b"WM_DELETE_WINDOW", 0), 0
    x11.XSendEvent(dpy, win, 0, 0, ctypes.byref(ev))
    x11.XFlush(dpy)
    print("sent WM_DELETE_WINDOW")

dpy = x11.XOpenDisplay(None)
cmd, pid = sys.argv[1], int(sys.argv[2])
win = find(dpy, x11.XDefaultRootWindow(dpy), pid)
if not win:
    sys.exit("window not found")
if cmd == "shot":
    shot(dpy, win, sys.argv[3])
elif cmd == "click":
    click(dpy, win, int(sys.argv[3]), int(sys.argv[4]))
elif cmd == "close":
    close(dpy, win)
elif cmd == "resize":
    x11.XResizeWindow.argtypes = [ctypes.c_void_p, ctypes.c_ulong, ctypes.c_uint, ctypes.c_uint]
    x11.XResizeWindow(dpy, win, int(sys.argv[3]), int(sys.argv[4]))
    x11.XFlush(dpy)
    print(f"resized to {sys.argv[3]}x{sys.argv[4]}")
