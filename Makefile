.PHONY: client server

all: client server

reformat:
	find client/ -iname *.h -o -iname *.hpp -o -iname *.cpp | xargs clang-format -i
	find server/ -iname *.h -o -iname *.hpp -o -iname *.cpp | xargs clang-format -i
	find common/ -iname *.h -o -iname *.hpp -o -iname *.cpp | xargs clang-format -i

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
	
