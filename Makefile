all:
	gcc -std=c99 -lreadline -lwiringPi dumper.c -o pitari-dumper
clean:
	rm -f pitari-dumper
