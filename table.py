"""
Description of the table and its parts.

Classes in this file include all the used lengths, colors, etc.
Moreover, they include the results of all the pre-computations
which depend only on these values (and not on the analyzed video).

For example, classes in this file include all the transformation matrices
used to change reference frame from, say, relative to a particular foosmen
into table coordinates.
"""

from numpy import pi as PI
from UserList import UserList
import transformation

class Table:

    def __init__(self):
        self.ground = Ground()
        self.rods = RodSet()
        self.foosmen = FoosmanSet()
        self.teams = [
            Team(0,  "red"),
            Team(1, "blue"),
        ]
        self.teams_by_name = {t.name: t for t in self.teams}
        
        rods_map = [
            (0,  "red", "goalkeeper"),
            (1,  "red", "defence"),
            (2, "blue", "attack"),
            (3,  "red", "midfield"),
            (4, "blue", "midfield"),
            (5,  "red", "attack"),
            (6, "blue", "defence"),
            (7, "blue", "goalkeeper"),
        ]
        
        for index, team_name, rod_name in rods_map:
            team = self.teams_by_name[team_name]
            rod_type = self.rods.types_by_name[rod_name]
            
            rod = Rod(index, team, rod_type)
            self.rods.append(rod)
            
            # FIXME: blue team rods are reversed
            team.rods.append(rod)
        
class Ground:

    def __init__(self):
        self.size = (1.135, 0.7)
        self.color = (0.2, 0.7, 0.2)

class RodSet(UserList):
    """
    Properties common to all the rods.
    """
    
    def __init__(self):
        super(RodSet, self).__init__()
    
        self.diameter = 0.015
        self.location_z = 0.085
        self.distance = d = 0.15
        self.color = (0.6, 0.6, 0.6)
        
        self.types = [
            RodType(0, "goalkeeper", -3.5 * d, [
                -0.21,
                 0.00,
                +0.21,
            ], 0.13),
            RodType(1, "defence", -2.5 * d, [
                -0.12,
                +0.12,
            ], 0.22),
            RodType(2, "midfield", -0.5 * d, [
                -0.24,
                -0.12,
                 0.00,
                +0.12,
                +0.24,
            ], 0.10),
            RodType(3, "attack", +1.5 * d, [
                -0.21,
                 0.0,
                +0.21,
            ], 0.13),
        ]
        
        self.types_by_name = {t.name: t for t in self.types}

class RodType:
    """
    Properties common to a rod type (goalkeeper, defence, midfield, attack).
    There are two rods for each type, one per team.
    """

    def __init__(self, index, name, location_x, foosmen_locations, translation_max):
        self.index = index
        self.name = name

        self.location_x = location_x
        
        self.transform = transformation.translate(location_x, 0)
        
        self.translation_max = translation_max
        self.rotation_max = PI
        
        self.foosmen = [FoosmanType(self, i, y) for i, y in enumerate(foosmen_locations)]
        
class FoosmanType:
    
    def __init__(self, rod, index, location_y):
        self.rod = rod
        self.index = index
        self.location_y = location_y
        
        self.transform = transformation.translate(0, location_y)
        
class FoosmanSet:
    """
    Properties common to all the foosmen.
    """
    
    def __init__(self):
        self.width = 0.05
        self.head_height = 0.02
        self.feet_height = 0.08

class Team:

    def __init__(self, index, name):
        self.index = index
        self.name = name
        
        if index == 0:
            self.transform = transformation.scale( 1, 1)
        else:
            self.transform = transformation.scale(-1,-1)

        self.foosmen = TeamFoosmanSet(index)
        self.rods = []

class TeamFoosmanSet(UserList):
    """
    Properties common to all the foosmen of a single team.
    """    

    def __init__(self, index):
        super(TeamFoosmanSet, self).__init__()
        
        if index == 0:
            self.color = (0.05, 0.05, 0.50)
        else:
            self.color = (0.50, 0.15, 0.05)

class Rod:

    def __init__(self, index, team, rod_type):
        self.index = index
        self.team = team
        self.type = rod_type
        self.transform = team.transform.dot(rod_type.transform)
        self.foosmen = [Foosman(f.index, self, f) for f in self.type.foosmen]

class Foosman:

    def __init__(self, index, rod, foosman_type):
        self.index = index
        self.type = foosman_type
        self.transform = rod.transform.dot(foosman_type.transform)

class Ball:

    def __init__(self):
        self.diameter = 0.035
        self.color = (0.85, 0.85, 0.85)

# Smoke test
if __name__ == "__main__":
    table = Table()

