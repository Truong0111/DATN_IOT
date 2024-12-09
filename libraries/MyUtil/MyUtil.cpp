#include "MyUtil.h"

void split_message_mqtt(String str, String delimiter, String arr[], int &arrSize) {
  int start = 0;
  int index = 0;
  int delimiterLength = delimiter.length();

  while (start < str.length()) {
    int end = str.indexOf(delimiter, start);

    if (end == -1) {
      arr[index] = str.substring(start);
      break;
    } else {
      arr[index] = str.substring(start, end);
      start = end + delimiterLength;
    }
    
    index++;
  }

  arrSize = index;
}