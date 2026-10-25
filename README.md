# ClashFree_Sem_3
Classes timetable generator that tries to  solve the scheduling problem.Built as a C++ PBL project.

Models :

Class Timeslot 
    an class to store the time and day 
    uses enum day class 

Class Courses 
    an class to store courses ,
    id , name , load hour and course type

Class Section 
    an class to store sections 
    id, name , student_count and list of course objects

    enrollCourses() is used to add courses to a particular section . 

Class Faculty 
    an class to store faculty 
    id , name , max_load weekly , list of timeslots which are availavle

    addSlot() is used to add available timeslot


Graph : 

class Node 
    stores ONE weekly session ( one hour ) of a course , section , faculty combination and a seperate timeslot
    sessionIndex says which session it is , getSessionCount() gives how many the course has
    stores set of all neighbour node* 

    addNeibour() to add a node into the set 
    assign(),unassign() to assign or unassign timeslot

class graph
    unique pointer list of all nodes , id

    add_node() to add node to the unique pointer list
    buildedged() to build the conflict graph edges
    exportedges() to export the edge_list 
    conflic() to check conflict between two nodes 

Class Room 
    an class to store rooms 
    id , name , capacity and room type ( THEORY or LAB )

    fits() checks if a section of given size can sit in the room 
    not used by any algorithm yet 

Class TimeSlotPool 
    stores the list of all possible timeslots in a week ( the colors for the graph )
    built from number of days and periods per day , never changed after that 

    all() gives the full list , size() gives number of slots 
    at() gives the slot at an index , indexOf() gives the index of a slot , contains() checks a slot is in the pool 

Class Dataset 
    owns all the course , faculty , section and room objects that are loaded 
    stores id lookups , the list of (section , course , faculty) links and days / periods per day 
    cannot be copied , nothing is ever removed from it so pointers stay valid 

    addCourse() , addFaculty() , addSection() , addRoom() to add data ( duplicate ids are rejected ) 
    addSectionCourse() to link a course and its faculty to a section 
    getCourse() , getFaculty() , getSection() , getRoom() to look up by id 
    validate() returns a list of problems found in the loaded data ( also checks hours : faculty hours vs maxLoad and available slots , section hours vs slots in the week , loadHours below 1 ) 


Loader : 

class DataLoader 
    reads a json file and fills a Dataset 

    loadFromFile() reads faculty , courses , sections , optional rooms and optional config 


Graph ( added ) : 

    buildFromDataset() in graph creates loadHours nodes for every (section , course , faculty) link ( one per weekly session , labs are also one hour each for now ) and then builds the edges 
    sessions of the same link are neighbours already ( same section ) , so they always get different slots 
    sem3 has 127 links = 425 nodes 


Scheduler : 

class Scheduler 
    abstract class , stores nothing 
    every algorithm ( greedy , dsatur , backtracking ) will inherit from it 

    schedule() takes the graph and the timeslotpool and returns a ScheduleResult , the graph is not changed 
    name() gives the algorithm name for output 

struct ScheduleResult 
    timetable , success , slots used and runtime in ms 

class Timetable 
    stores which timeslot every node got , as an index into the TimeSlotPool ( -1 means not assigned )
    cheap to copy , so algorithms can try something and go back 

    assign() , unassign() to set or remove the slot of a node 
    isAssigned() , slotOf() , slotAt() to read the slot of a node 
    isComplete() , assignedCount() , slotsUsed() , unassignedNodes() , nodesAt() to check the result 


Data and tools : 

data.json , sem3.json 
    input files for the loader , sem3.json has all the b.tech cse sem 3 sections , core courses only 

csv/ 
    the same sem 3 data as csv files ( faculty , courses , sections , section_courses , rooms , config , unavailable )

build_dataset.py 
    generates sem3 json and the csv files 

visualise.py 
    draws the conflict graph from a json file and edges.txt 
    python visualise.py sem3.json , options --section , --faculty , --labels , --out 

app.py 
    streamlit app , upload the csv files , check or submit them , runs the c++ program on them 
    the timetable shown is empty for now , no algorithm is built yet 
    streamlit run app.py 


How to run : 

    g++ -std=c++17 -I. main.cpp -o clashfree 
    ./clashfree sem3.json 

    writes edges.txt , the edge list of the conflict graph 

