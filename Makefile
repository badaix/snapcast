.PHONY: client server

all: client server

server:
	$(MAKE) -C server

client:
	$(MAKE) -C client
	
clean:
	$(MAKE) clean -C client 
	$(MAKE) clean -C server 
	rm -f *~
	
installclient:
	$(MAKE) install -C client 

installserver:
	$(MAKE) install -C server 

uninstallclient:
	$(MAKE) uninstall -C client 

uninstallserver:
	$(MAKE) uninstall -C server 
	
