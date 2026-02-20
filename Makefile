

kernec: kernec.c
	gcc $< -o $@

run: kernec
	./kernec
