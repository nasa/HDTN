#ifndef _HDTN_QUEUE_H
#define _HDTN_QUEUE_H

namespace hdtn {
struct queueEntry {
    int data;
    queueEntry* next;
    queueEntry(int d) {
        data = d;
        next = NULL;
    }
};

struct queue {
    queueEntry *front, *rear;
    queue() {
        front = rear = NULL;
    }

    void enQueue(int x) {
        // Create a new LL node
        queueEntry* temp = new queueEntry(x);

        // If queue is empty, then
        // new node is front and rear both
        if (rear == NULL) {
            front = rear = temp;
            return;
        }

        // Add the new node at
        // the end of queue and change rear
        rear->next = temp;
        rear = temp;
    }

    // Function to remove
    // a key from given queue q
    void deQueue() {
        // If queue is empty, return NULL.
        if (front == NULL)
            return;

        queueEntry* temp = front;
        front = front->next;

        if (front == NULL)
            rear = NULL;

        delete (temp);
    }
};

}  // namespace hdtn

#endif
