//
// Implement the List class
//

#include <stdio.h>
#include "List.h"
#include <iostream>

//
// Inserts a new element with value "val" in
// ascending order.
//
void
List::insertSorted( int val )
{
  ListNode *Node = new ListNode();
  Node->_value = val;
  Node->_next = nullptr;
  ListNode *temp;

  if (_head->_next == nullptr) { //d
    _head->_next = Node; //d
  } else if (_head->_next->_value >= Node->_value){
    Node->_next = _head->_next;
    _head->_next = Node;
  } else {
    temp = _head->_next;
    while (temp->_next != nullptr && temp->_next->_value < Node->_value){
      temp = temp->_next;
    }
    Node->_next = temp->_next;
    temp->_next = Node;
  }
}

//
// Inserts a new element with value "val" at
// the end of the list.
//
void
List::append( int val )
{
  ListNode *Node = new ListNode();
  Node->_value = val;
  Node->_next = nullptr;
  ListNode *temp;
  ListNode *temp2;

  temp = _head->_next;
  while (temp != nullptr) {
    temp2 = temp;
    temp = temp->_next;
  }
  temp2->_next = Node;
}

//
// Inserts a new element with value "val" at
// the beginning of the list.
//
void
List::prepend( int val )
{
  ListNode *Node = new ListNode();
  Node->_value = val;
  Node->_next = nullptr;

  Node->_next = _head->_next;
  _head->_next = Node;
}

// Removes an element with value "val" from List
// Returns 0 if succeeds or -1 if it fails
int 
List:: remove( int val )
{
  // Complete procedure
  ListNode *temp;
  ListNode *temp2;
  temp = _head->_next;
  temp2 = _head;

  while (temp != nullptr) {
    if (temp->_value == val){
      temp2->_next = temp->_next;
      delete temp;
      return 0;
    }
    temp2 = temp;
    temp = temp->_next;
  }
  return -1;

}

// Prints The elements in the list. 
void
List::print()
{
  // Complete procedure 
  ListNode *temp = _head->_next;
  while (temp != nullptr){
    std::cout << temp->_value << " ";
    temp = temp->_next;
  }
  std::cout << std::endl;
}

//
// Returns 0 if "value" is in the list or -1 otherwise.
//
int
List::lookup(int val)
{
  // Complete procedure 
  ListNode *temp = _head->_next;
  while (temp != nullptr) {
    if (temp->_value == val){
      return 0;
    }
    temp = temp->_next;
  }
  return -1;
}

//
// List constructor
//
List::List()
{
  // Complete procedure 
  _head = new ListNode();
  _head->_value = 0;
  _head->_next = nullptr;
}

//
// List destructor: delete all list elements, if any.
//
List::~List()
{
  // Complete procedure 
  ListNode *ptr = _head->_next;
  delete this->_head;
  while (ptr != nullptr) {
    ListNode *temp = ptr;
    delete temp;
    ptr = ptr->_next;
  }
}
