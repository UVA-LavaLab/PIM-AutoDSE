all:
	 g++ test.cpp -DFUSED=1 -L./ -lpimeval -o test.out

fused:
	 g++ test.cpp -DFUSED=1 -L./ -lpimeval -o test.out

unfused:
	 g++ test.cpp -DUNFUSED=1 -L./ -lpimeval -o test.out

clean:
	rm -rf ./*.out
	rm -rf ./*_test.cpp
	rm -rf ErrorLog.txt
	rm -rf ./*_unfused_log.txt
	rm -rf ./*_fused_log.txt
	rm -rf *_valid.cpp
	rm -rf ./*.log
	rm -rf ./opentuner* 
