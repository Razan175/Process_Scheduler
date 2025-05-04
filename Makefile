build:
	gcc process_generator.c -o process_generator.out
	gcc clk.c -o clk.out
	gcc scheduler.c -o scheduler.out -lm
	gcc process.c -o process.out
	gcc test_generator.c -o test_generator.out
clean:
	rm -f *.out  processes.txt

all: clean build

run:
<<<<<<< HEAD
	./process_generator.out processes.txt 1 2
=======
	./process_generator.out processes.txt
>>>>>>> 1b75e691e9b3a03d951981109a71020fd973105b
