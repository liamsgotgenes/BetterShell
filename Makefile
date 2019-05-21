ALL:
	gcc -g main.c -o shell
clean:
	rm shell*
	sudo rm /usr/bin/shell
install:
	gcc main.c -o shell
	sudo cp shell /usr/bin/shell
