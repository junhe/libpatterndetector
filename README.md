libpatterndetector
==================

A lib to find I/O patterns. The algorithm is described in paper:
 Jun He, John Bent, Aaron Torres, Gary Grider, Garth Gibson, Carlos Maltzahn, and Xian-He Sun, 
 "I/O Acceleration with Pattern Detection", 
 accepted to appear in Proc. of the 22th International 
 ACM Symposium on High Performance Distributed Computing (HPDC'13), 
 New York City, NY, June 2013.

*** Directory Tree
pattern.h pattern.cpp ....... the key library files. They do not depend on other files or library (except C++ STL). 
example.cpp ................. an example demostrating the usage of the pattern detector.

*** Usage
1. Copy pattern.h and pattern.cpp to your project;
2. Include pattern.h in the file where you want to do pattern detection;
3. Call the pattern detection functions you need to call;
4. Include pattern.h and pattern.cpp when you compile;
Anyway, the following example shows how.

*** Build the Example
First, make sure you have g++ :)
Then, do
> $make
This will build the example 

*** Run the Example
> $./xexample
You will see the output showing the pattern.

The boundary handling is slightly different from the one described in the paper.



It is a part of JIOPAT project. For more information, please visit http://junhe.github.io/jiopat/ 




