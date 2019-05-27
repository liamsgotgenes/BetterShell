ALL:
	gcc -g main.c input.c -o shell
clean:
	rm shell*
	sudo rm /usr/bin/shell
install:
	gcc main.c input.c -o shell
	sudo cp shell /usr/bin/shell
