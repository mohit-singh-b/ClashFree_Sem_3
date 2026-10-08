import json
import networkx as nx
import matplotlib.pyplot as plt

with open('data.json', 'r') as f:
    data = json.load(f)

course_names = {c['id']: c['name'] for c in data['courses']}

labels = {}
node_id = 0
for section in data['sections']:
    sec_name = section['name']
    for course in section['courses']:
        c_name = course_names[course['courseId']]
        f_id = course['facultyId']
        
        labels[node_id] = f"{sec_name}\n{c_name[:6]}...\n(T: {f_id})"
        node_id += 1


G = nx.read_edgelist('edges.txt', nodetype=int)


plt.figure(figsize=(12, 10))
pos = nx.spring_layout(G, k=0.9, seed=42) 

nx.draw(G, pos, 
        labels=labels,          
        with_labels=True, 
        node_color='lightblue', 
        node_size=2500,         
        font_size=8,
        font_weight='bold', 
        edge_color='gray')

plt.title("C++ Graph Structure (with JSON Labels)")
plt.show()