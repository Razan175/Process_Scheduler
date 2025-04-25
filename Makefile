build:
	gcc process_generator.c -o process_generator.out
	gcc clk.c -o clk.out
	gcc scheduler.c -o scheduler.out
	gcc process.c -o process.out
	gcc test_generator.c -o test_generator.out
	gcc RR.c -o RR.out
	gcc SRTN.c -o SRTN.out
	gcc HPF.c -o HPF.out

clean:
	rm -f *.out  processes.txt

all: clean build

run:
	./process_generator.out
