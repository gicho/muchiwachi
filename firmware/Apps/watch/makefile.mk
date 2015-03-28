########################################################################
# Add Application sources here.
########################################################################
APP_SRC = accel.c adverts.c buttons.c display.c gestures.c groupfsm.c main.c mainfsm.c spi.c store.c \
      $(U8GLIB)

U8GLIB = u8glib/u8g_bitmap.c \
    u8glib/u8g_circle.c \
    u8glib/u8g_clip.c \
    u8glib/u8g_com_api_16gr.c \
    u8glib/u8g_com_api.c \
    u8glib/u8g_com_i2c.c \
    u8glib/u8g_com_io.c \
    u8glib/u8g_com_null.c \
    u8glib/u8g_cursor.c \
    u8glib/u8g_delay.c \
    u8glib/u8g_ellipse.c \
    u8glib/u8g_font.c \
    u8glib/u8g_font_data.c \
    u8glib/u8g_line.c \
    u8glib/u8g_ll_api.c \
    u8glib/u8g_page.c \
    u8glib/u8g_pb8h1.c \
    u8glib/u8g_pb.c \
    u8glib/u8g_polygon.c \
    u8glib/u8g_rect.c \
    u8glib/u8g_rot.c \
    u8glib/u8g_scale.c \
    u8glib/u8g_state.c \
    u8glib/u8g_u16toa.c \
    u8glib/u8g_u8toa.c \
    u8glib/u8g_virtual_screen.c \
    u8glib/dev/u8g_dev_ls010b7dh01.c \
    u8glib/sys/u8g_bcm2073x.c 

APP_PATCHES_AND_LIBS += rtc_api.a

########################################################################
################ DO NOT MODIFY FILE BELOW THIS LINE ####################
########################################################################
