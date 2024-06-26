#Hi, and welcome to one of my code examples.

This code is a part of my own game engine project. It contains two pieces of header files. "Event.h" contains two template classes to achieve a generic delegate system to support both member functions and global functions with variadic arguments:

1-) class Event -> Global Functions
2-) class LinkedEvent -> Member Functions

"PTR.h" contains another template class "PTR". This allows me to store and call different member functions of the classes. It is also useful for such things like garbage collection.