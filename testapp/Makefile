#
# Makefile
#

APP = i2c_usb

all: $(APP)

clean:
	rm -f $(APP)

$(APP): $(APP).c
	$(CC) -Wall -o $@ $(APP).c -lusb

install:
	install $(APP) $(DESTDIR)/usr/bin
	install i2c_tiny_usb.rules $(DESTDIR)/etc/udev/rules.d
	install i2c_tiny_usb.desktop $(DESTDIR)/usr/share/applications/hildon
