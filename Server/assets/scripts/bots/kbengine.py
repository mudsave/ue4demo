# -*- coding: utf-8 -*-
import KBEngine
from KBEDebug import *

from Extra.QuestLoader import g_quests
from Extra.SkillLoader import g_skillLoader

g_quests.loadQuest("scripts/data/QuestConfig.json")
g_skillLoader.loadSkill("scripts/data/SkillConfig.json")

playerRole = []

def onInit(isReload):
	"""
	KBEngine method.
	当引擎启动后初始化完所有的脚本后这个接口被调用
	@param isReload: 是否是被重写加载脚本后触发的
	@type isReload: bool
	"""
	DEBUG_MSG('onInit::isReload:%s' % isReload)

def onStart():
	"""
	KBEngine method.
	在onInitialize调用之后， 准备开始游戏时引擎调用这个接口.
	"""
	pass
	
def onFinish():
	"""
	KBEngine method.
	客户端将要关闭时， 引擎调用这个接口
	可以在此做一些游戏资源清理工作
	"""
	pass

def getPlayer(id):
	"""
	获取某个机器人
	"""
	for e in playerRole:
		if e.id == id:
			return e
	return None