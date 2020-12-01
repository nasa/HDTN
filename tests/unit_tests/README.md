The unit tests use the Google Test Framework.  


Google Test was installed using the following steps:

	1.	Get Google Test source files:
	
			sudo apt-get install libgtest-dev
			
	2.	Install cmake if it is not alread installed with:  
	
			sudo apt-get install cmake
			
	3.	Build Google Test:
	
			cd /usr/src/gtest
			sudo cmake CMakeLists.txt
			sudo make
			
	4.	Copy the libraries to a default location:
	
			sudo cp /usr/src/gtest/lib/lib/*.a /usr/local/lib

	5.	Copy the include directories to a default location:
	
			sudo cp -r /usr/src/gtest/include/gtest /usr/local/include/
			
Similarly, Google Mock was installed using the following steps:

	1.	Get Google Mock source files:
	
			sudo apt-get install google-mock
			
	2.	Install cmake if it is not alread installed with:  
	
			sudo apt-get install cmake
			
	3.	Build Google Mock:
	
			cd /usr/src/gmock
			sudo cmake CMakeLists.txt
			sudo make
			
	4.	Copy the libraries to a default location:
	
			sudo cp /usr/src/gmock/lib/lib/*.a /usr/local/lib

	5.	Copy the include directories to a default location:
	
			sudo cp -r /usr/src/gmock/include/gmock /usr/local/include/
			
