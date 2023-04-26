client1: 
	gcc c1.c -o c1 
	./c1 

client2: 
	gcc c2.c -o c2 
	./c2 

server: 
	gcc s1.c -o s1 
	./s1

test:
	gcc test.c -o test_c 
	./test_c
