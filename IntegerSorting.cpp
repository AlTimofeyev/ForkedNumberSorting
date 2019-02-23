/*
  Author: Al Timofeyev
	Date:   1/19/2019
	Desc:   Reads file of integers with delimiters from file and parses
          them into an array. Then sorts that array by splitting the
          sorting up to different processes using fork(). Merging of
          separate parts is handled by the parent process.
*/


#include <iostream>
#include <vector>
#include <fstream>
#include <unistd.h>     // For fork()
#include <sys/types.h>  // For fork() and pid_t
#include <sys/wait.h>		// For waitpid()

using namespace std;

// Function Prototypes.
void parseInts(string str, string delimiter, vector<int> &numList);
void sortInts(vector<int> numberList, int write_fd);
void printInts(vector<int> list);
void convertVectToInt(vector<int> &vect, int *arr);
void convertIntToVect(vector<int> &vect, int *arr, int size);
int partition(vector<int> &vect, int startIndex, int endIndex);
void quicksort(vector<int> &vect, int startIndex, int endIndex);
void merge(vector<int> &combined, vector<int> leftV, vector<int> rightV);

static vector<int> numList;

int main(int argc, char* argv[])
{
  // This part makes sure that there is a file and a delimiter.
  // Filename is stored in argv[1], delimiter is stored in argv[2].
  if(argc < 3)
  {
    cout << "Missing Parameters: Needs a filename and delimiters." << endl;
    cout << "\tExample: <program name> numbers.txt @.*/\n" << endl;
    return 0;
  }

  // Set the filename and delimiter.
  string filename = argv[1];
  string delimiter = argv[2];

  // Open the file an make sure it exists.
  ifstream inputFile(filename);
  if(inputFile.fail())
  {
    cout << "Failed to open file: " << filename << endl;
    return 0;
  }

  // Read the file and parse the integers into numList.
  string line;
  while(!inputFile.eof())
  {
    getline(inputFile, line);
    parseInts(line, delimiter, numList);
  }

  inputFile.close();

  // If the file was empty.
  if(numList.size() < 1)
  {
    cout << "There are no integers to sort." << endl;
    cout << "The file " << filename << " is empty." << endl;
  }
  // If only one integer exists, no sorting needed.
  else if(numList.size() == 1)
  {
    cout << "No sorting is needed, there is only one integer: ";
    cout << to_string(numList[0]) << endl;
  }
  else
  {
    cout << "Start sorting list: ";
    printInts(numList);
    cout << endl;

    sortInts(numList, 1);

    cout << "Finished sorting list: ";
    printInts(numList);
  }

  return 0;
}


/*
  Parses/splits a string of integers using the provided delimiter and
  stores each integer in numList.
  @param str The string that contains the integers.
  @param delimiter The string of delimiters used to split str.
  @param numList The integer vector list where to store the converted ints.
*/
void parseInts(string str, string delimiter, vector<int> &numList)
{
  int startIndex = 0;
  int endIndex = str.find_first_of(delimiter, startIndex);

  while(endIndex < str.size() && endIndex != string::npos)
  {
    // Only convert to int and store in numList if start/end index aren't equal.
    if(startIndex != endIndex)
      numList.push_back(stoi(str.substr(startIndex, endIndex - startIndex)));

    // Update the start and end indexes.
    startIndex = endIndex + 1;
    endIndex = str.find_first_of(delimiter, startIndex);
  }

  // Add the last element to the numList.
  if(startIndex < str.size())
    numList.push_back(stoi(str.substr(startIndex, endIndex - startIndex)));

  //printInts(numList); // FOR DEBUGGING PRINTS ALL ELEMENTS
}


/*
  Sort the integers in numList using Divide and Conquor method
  and fork() and pipe().
*/
void sortInts(vector<int> numberList, int write_fd)
{
  // The status is for waitpid for the two child processes.
  int status1;
  int status2;

  // Prep piping, fork, and vector<in> child variables.
  int pipefd[2];
  pid_t leftcpid, rightcpid;
  vector<int> childnums(numberList.size()/2);

  // Sort and send to parent through pipe.
  if(numberList.size() <= 5)
  {
    // Sort the numbers an convert to int array.
    quicksort(numberList, 0, numberList.size());
    int sortedNums[numberList.size()];
    convertVectToInt(numberList, sortedNums);

    // Write the sorted list to pipe and exit.
    write(write_fd, sortedNums, (sizeof(int)*numberList.size()));
    exit(0);
  }

  // If the piping fails (?).
  if(pipe(pipefd) == -1)
  {
    perror("Failed");
  }

  // The parent forks the process to two children.
  if(!(leftcpid = fork()) || !(rightcpid = fork()))
  {
    // If it's the left child.
    if(leftcpid == 0)
    {
      // Copy the respective part of numberList into childnums.
      copy(numberList.begin(), numberList.begin()+(numberList.size()/2), childnums.begin());
    }
    // Else if it's the right child.
    else
    {
      // If the numberList has odd number of elements, then add 1 to childnums size.
      if(numberList.size() % 2 != 0)
        childnums.resize(childnums.size()+1);

      // Copy the respective part of numberList into childnums.
      copy(numberList.begin()+(numberList.size()/2), numberList.end(), childnums.begin());
    }

    // Recursive call.
    sortInts(childnums, pipefd[1]);
  }

  // Else in Parent process.
  {
    // Wait for both child processes to finish execution.
    waitpid(leftcpid, &status1, 0);
    waitpid(rightcpid, &status2, 0);

    // Prep sizes of int arrays.
    int lNumsSize = numberList.size()/2;
    int rNumsSize = numberList.size()/2;
    if(numberList.size() % 2 != 0)
      rNumsSize++; // If numberList is odd, then right size needs to be increased by 1.

    // Initialize two int arrays with proper size.
    int leftNums[lNumsSize];
    int rightNums[rNumsSize];

    // Read the sorted left and right values from pipe.
    read(pipefd[0], leftNums, sizeof(int)*lNumsSize);
    read(pipefd[0], rightNums, sizeof(int)*rNumsSize);

    // Convert the leftNums and rightNums to vector arrays.
    vector<int> left, right;
    convertIntToVect(left, leftNums, lNumsSize);
    convertIntToVect(right, rightNums, rNumsSize);

    // Merge the sorted lists from child processes.
    vector<int> combined;
    merge(combined, left, right);

    // Print intermediate results.
    cout << "In Parent:" << endl;
    cout << "Printing left:\t";
    printInts(left);
    cout << "Printing right:\t";
    printInts(right);
    cout << "COMBINED:\t";
    printInts(combined);
    cout << endl;

    // If back in the very first Parent process.
    if(numberList.size() != numList.size())
    {
      // Convert numberList vector to int array.
      int sortedNums[numberList.size()];
      convertVectToInt(combined, sortedNums);

      // Write the merged and sorted list to pipe and exit.
      write(write_fd, sortedNums, (sizeof(int)*combined.size()));
      exit(0);
    }

    copy(combined.begin(), combined.end(), numList.begin());
  }
}


/*
  Prints out all the integers of a vecotr array.
*/
void printInts(vector<int> list)
{
  for(auto x : list)
    cout << x << " ";
  cout << endl;
}

/*
  Convert a vector<int> array to a normal integer array.
  Both parameters must already be of the same size.
  @param vect The vector array with all the values.
  @param arr Empty integer array.
*/
void convertVectToInt(vector<int> &vect, int *arr)
{
  for(int i = 0; i < vect.size(); i++)
    arr[i] = vect[i];
}

/*
  Convert a normal integer array to a vector<int> array.
  The vector array doesn't need to have a declared size.
  @param vect Empty vector arrray.
  @param arr The integer array with all the values.
  @param size The size of the normal integer array.
*/
void convertIntToVect(vector<int> &vect, int *arr, int size)
{
  for(int i = 0; i < size; i++)
    vect.push_back(arr[i]);
}

// ******* Sorting Implementations Below *******

/*
  Partitions the vector by picking last element as pivot.
  @param startIndex Starting index (inclusive).
  @param endIndex Ending index (exclusive).

  @return The new index of the pivot in vect.
*/
int partition(vector<int> &vect, int startIndex, int endIndex)
{
  int pivot = vect[endIndex-1];
  int i = startIndex; // i will be the index of the smaller element.

  for(int j = startIndex; j < endIndex; j++)
  {
    if(vect[j] <= pivot)
    {
      iter_swap(vect.begin()+i, vect.begin()+j);
      i++;
    }
  }

  //iter_swap(vect.begin()+i, vect.begin()+endIndex-1);

  return --i;
}

/*
  The quicksort function.
  @param vect The vector array to sort.
  @param startIndex The starting index of array (inclusive).
  @param endIndex The ending index of array (exclusive).
*/
void quicksort(vector<int> &vect, int startIndex, int endIndex)
{
  if(startIndex < endIndex)
  {
    int partIndex = partition(vect, startIndex, endIndex);

    quicksort(vect, startIndex, partIndex);
    quicksort(vect, partIndex+1, endIndex);
  }
}

// ******* Merge Function *******
/*
  This function only works with two arrays that have already been sorted.
  @param combined The empty vector array where merged values are stored.
  @param leftV The sorted left vector array.
  @param rightV The sorted right vector array.
*/
void merge(vector<int> &combined, vector<int> leftV, vector<int> rightV)
{
  // Make doubley sure that both leftV and rightV are sorted.
  quicksort(leftV, 0, leftV.size());
  quicksort(rightV, 0, rightV.size());

  int i = 0;  // starting index of left.
  int j = 0;  // starting index of right.

  // Continuously merge only the smallest value first.
  while (i < leftV.size() && j < rightV.size())
  {
    if(leftV.at(i) <= rightV.at(j))
    {
      combined.push_back(leftV.at(i));
      i++;
    }
    else
    {
      combined.push_back(rightV.at(j));
      j++;
    }
  }

  // Merge any remaining values left in either leftV or rightV vectors.
  while(i < leftV.size())
  {
    combined.push_back(leftV.at(i));
    i++;
  }
  while(j < rightV.size())
  {
    combined.push_back(rightV.at(j));
    j++;
  }
}
