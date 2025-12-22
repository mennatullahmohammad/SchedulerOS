build:
	gcc process_generator.c headers.c -o process_generator.out
	gcc clk.c -o clk.out
	gcc scheduler.c MMU.c headers.c  -lm -o scheduler.out
	gcc process.c headers.c -o process.out

clean:
	rm -f *.out processes.txt

all: clean build

run:
	./process_generator.out