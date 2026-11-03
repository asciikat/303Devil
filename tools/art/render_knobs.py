"""Render original, unmarked physical knob assets with Blender 4.x.
Run: blender -b --factory-startup --python tools/art/render_knobs.py -- assets
The plugin projects its live pointer over the cap, so assets never bake in values.
"""
import bpy, math, sys
from pathlib import Path
from mathutils import Vector
from bpy_extras.object_utils import world_to_camera_view
out = Path(sys.argv[sys.argv.index('--')+1] if '--' in sys.argv else 'assets')
out.mkdir(parents=True, exist_ok=True)
bpy.ops.object.select_all(action='SELECT'); bpy.ops.object.delete(use_global=False)
scene=bpy.context.scene
scene.render.engine='CYCLES'; scene.cycles.device='CPU'; scene.cycles.samples=64
scene.cycles.use_denoising=False
scene.render.resolution_x=384;scene.render.resolution_y=384;scene.render.resolution_percentage=100
scene.render.film_transparent=True
scene.render.image_settings.file_format='PNG';scene.render.image_settings.color_mode='RGBA'
scene.world.color=(0.18,0.18,0.18)
scene.view_settings.view_transform='Standard'
scene.view_settings.exposure=-0.6
def material(name,col,rough,metal=0):
 m=bpy.data.materials.new(name);m.use_nodes=True
 bs=m.node_tree.nodes.get('Principled BSDF');bs.inputs['Base Color'].default_value=(*col,1)
 bs.inputs['Roughness'].default_value=rough;bs.inputs['Metallic'].default_value=metal
 noise=m.node_tree.nodes.new('ShaderNodeTexNoise');noise.inputs['Scale'].default_value=180
 bump=m.node_tree.nodes.new('ShaderNodeBump');bump.inputs['Strength'].default_value=.12;bump.inputs['Distance'].default_value=.008
 m.node_tree.links.new(noise.outputs['Fac'],bump.inputs['Height']);m.node_tree.links.new(bump.outputs['Normal'],bs.inputs['Normal'])
 return m
black=material('Moulded charcoal ABS',(.016,.019,.022),.32)
rim=material('Satin aluminium collar',(.18,.20,.21),.28,.75)
capmat=material('Painted cap',(.35,.75,.025),.52)
capmat.node_tree.nodes.get('Principled BSDF').inputs['Specular IOR Level'].default_value=.18
def cylinder(name,r,depth,z,mat,vertices=128):
 bpy.ops.mesh.primitive_cylinder_add(vertices=vertices,radius=r,depth=depth,location=(0,0,z))
 ob=bpy.context.object;ob.name=name;ob.data.materials.append(mat)
 bevel=ob.modifiers.new('Moulded edge','BEVEL');bevel.width=.025;bevel.segments=3
 ob.modifiers.new('Weighted normals','WEIGHTED_NORMAL')
 for poly in ob.data.polygons:poly.use_smooth=True
 return ob
cylinder('Lower mounting collar',1.02,.08,.04,rim)
cylinder('Knob wall',.97,.55,.34,black)
for i in range(64):
 a=i*2*math.pi/64
 bpy.ops.mesh.primitive_cube_add(size=1,location=(.968*math.cos(a),.968*math.sin(a),.34))
 o=bpy.context.object;o.name='Moulded grip';o.scale=(.055,.045,.49);o.rotation_euler.z=a
 bpy.ops.object.transform_apply(location=False,rotation=False,scale=True)
 o.data.materials.append(black)
 b=o.modifiers.new('Rounded grip','BEVEL');b.width=.012;b.segments=2
 o.modifiers.new('Grip normals','WEIGHTED_NORMAL')
cylinder('Cap rim',.96,.055,.625,rim)
cylinder('Inset cap',.89,.05,.663,capmat)
bpy.ops.object.camera_add(location=(0,-6,6))
camera=bpy.context.object;camera.rotation_euler=(Vector((0,0,.32))-camera.location).to_track_quat('-Z','Y').to_euler()
camera.data.type='ORTHO';camera.data.ortho_scale=2.85;scene.camera=camera
for loc,power,size in [((-3,-4,7),230,4),((4,-1,3),70,3),((0,4,5),120,3)]:
 bpy.ops.object.light_add(type='AREA',location=loc)
 l=bpy.context.object;l.data.energy=power;l.data.shape='DISK';l.data.size=size
 l.rotation_euler=(-l.location).to_track_quat('-Z','Y').to_euler()
for name,color in [('lime',(.42,.8,.025)),('pink',(.8,.018,.22)),('black',(.025,.03,.035))]:
 capmat.node_tree.nodes.get('Principled BSDF').inputs['Base Color'].default_value=(*color,1)
 scene.render.filepath=str((out/('knob_'+name+'.png')).resolve());bpy.ops.render.render(write_still=True)
for name,v in [('centre',(0,0,.69)),('right',(.89,0,.69)),('back',(0,.89,.69))]:
 p=world_to_camera_view(scene,camera,Vector(v));print('PROJECTION',name,p.x,1-p.y)
