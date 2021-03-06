#include "views/ViewController.h"
#include "Log.h"
#include "SystemData.h"
#include "Settings.h"

#include "views/gamelist/BasicGameListView.h"
#include "views/gamelist/DetailedGameListView.h"
#include "views/gamelist/VideoGameListView.h"
#include "views/gamelist/GridGameListView.h"
#include "guis/GuiMenu.h"
#include "guis/GuiMsgBox.h"
#include "animations/LaunchAnimation.h"
#include "animations/MoveCameraAnimation.h"
#include "animations/LambdaAnimation.h"
#include <SDL.h>
#include "mediaplayer/IAudioPlayer.h"
#include "guis/GuiContext.h"

namespace
{
	static const std::string k_easterEggSoundPath = getHomePath() + "/.emulationstation/easter_egg/toasty.wav";
	static const std::string k_easterEggImagePath = getHomePath() + "/.emulationstation/easter_egg/toasty.png";
}

ViewController* ViewController::sInstance = NULL;

ViewController* ViewController::get()
{
	assert(sInstance);
	return sInstance;
}

void ViewController::init(gui::Context& guiContext)
{
	assert(!sInstance);
	sInstance = new ViewController(guiContext);
}

ViewController::ViewController(gui::Context& guiContext)
	: GuiComponent(guiContext.GetWindow()),
	mCurrentView(nullptr),
	mCamera(Eigen::Affine3f::Identity()),
	mFadeOpacity(0),
	mLockInput(false),
	mEasterEggImage(nullptr)
{
	setIsPersistent(true);
	m_context = &guiContext;
	mState.viewing = NOTHING;
	mHasEasterEggImage = boost::filesystem::exists(k_easterEggImagePath);
	srand(time(NULL));
}

ViewController::~ViewController()
{
	assert(sInstance == this);
	sInstance = NULL;
}

void ViewController::goToStart()
{
	// TODO
	/* mState.viewing = START_SCREEN;
	mCurrentView.reset();
	playViewTransition(); */
	const std::string lastSystemSelectedName = Settings::getInstance()->getString("LastSystemSelected");
	SystemData* system = SystemData::GetSystemByName(lastSystemSelectedName);
	if (system && system->IsEnabled())
	{
		goToSystemView(system, "fade");
	}
	else
	{
		goToSystemView(SystemData::GetSystems().front());
	}

	InitBackgroundMusic();
}

void ViewController::InitBackgroundMusic()
{
	if (m_context && m_context->GetAudioPlayer())
	{
		const std::string musicFolder = getHomePath() + "/.emulationstation/music";
		std::vector<std::string> files;
		GetFilesInFolder(musicFolder, files);
		m_context->GetAudioPlayer()->AddToPlaylist(files, mediaplayer::ShuffleE::k_yes);
		m_context->GetAudioPlayer()->SetPlaybacktMode(mediaplayer::PlaybackModeE::k_loop);
		const int volume = Settings::getInstance()->getInt("BackgroundMusicVolume");
		m_context->GetAudioPlayer()->SetVolume(volume);
		if (Settings::getInstance()->getBool("BackgroundMusicEnabled"))
		{
			m_context->GetAudioPlayer()->StartPlaylist();
		}
	}
}


int ViewController::getSystemId(SystemData* system)
{
	std::vector<SystemData*> sysVec = SystemData::GetSystems();
	return std::find(sysVec.begin(), sysVec.end(), system) - sysVec.begin();
}

void ViewController::goToSystemView(SystemData* system)
{
	const std::string transition_style = Settings::getInstance()->getString("TransitionStyle");
	goToSystemView(system, transition_style);
}

void ViewController::goToSystemView(SystemData* system, const std::string transition_style)
{
	// Tell any current view it's about to be hidden
	if (mCurrentView)
	{
		mCurrentView->onHide();
	}


	mState.viewing = SYSTEM_SELECT;
	mState.system = system;

	CheckBGMusicState();

	auto systemList = getSystemListView();
	systemList->setPosition(getSystemId(system) * ( float ) Renderer::getScreenWidth(), systemList->getPosition().y());

	systemList->goToSystem(system, false);
	mCurrentView = systemList;

	playViewTransition(transition_style);
}

void ViewController::goToNextGameList()
{
	assert(mState.viewing == GAME_LIST);
	SystemData* system = getState().getSystem();
	assert(system);
	goToGameList(system->getNextEnabled());
}

void ViewController::goToPrevGameList()
{
	assert(mState.viewing == GAME_LIST);
	SystemData* system = getState().getSystem();
	assert(system);
	goToGameList(system->getPrevEnabled());
}

void ViewController::CheckBGMusicState()
{
	if (!m_context || !m_context->GetAudioPlayer())
	{
		return;
	}
	const bool musicEnabled = Settings::getInstance()->getBool("BackgroundMusicEnabled");
	const bool videoPreviewPauseBGMusic = Settings::getInstance()->getBool("VideoPreviewPauseBGMusic");
	const bool wasPlaylistStarted = m_context->GetAudioPlayer()->IsPaused() || m_context->GetAudioPlayer()->IsPlaying();
	if (!musicEnabled)
	{
		m_context->GetAudioPlayer()->Pause();
	}
	else
	{
		bool play = false;
		if (mState.viewing == GAME_LIST)
		{
			if (videoPreviewPauseBGMusic)
			{
				m_context->GetAudioPlayer()->Pause();
			}
			else
			{
				play = true;
			}
		}
		else
		{
			play = true;
		}
		if (play)
		{
			if (wasPlaylistStarted)
			{
				m_context->GetAudioPlayer()->Resume();
			}
			else
			{
				m_context->GetAudioPlayer()->StartPlaylist();
			}
		}
	}
}

void ViewController::goToGameList(SystemData* system)
{

	if (mState.viewing == SYSTEM_SELECT)
	{
		// move system list
		auto sysList = getSystemListView();
		float offX = sysList->getPosition().x();
		int sysId = getSystemId(system);
		sysList->setPosition(sysId * ( float ) Renderer::getScreenWidth(), sysList->getPosition().y());
		offX = sysList->getPosition().x() - offX;
		mCamera.translation().x() -= offX;
	}

	mState.viewing = GAME_LIST;
	mState.system = system;

	CheckBGMusicState();

	if (mCurrentView)
	{
		mCurrentView->onHide();
	}
	mCurrentView = getGameListView(system);
	if (mCurrentView)
	{
		mCurrentView->onShow();
	}
	const std::string transition_style = Settings::getInstance()->getString("TransitionStyle");
	playViewTransition(transition_style);
}

void ViewController::goToRandomGame()
{
	unsigned int total = 0;
	for (SystemData* system : SystemData::GetSystems())
	{
		if (system->getName() != "retropie")
			total += system->getDisplayedGameCount();
	}

	// get random number in range
	int target = std::lround(( ( double ) std::rand() / ( double ) RAND_MAX ) * total);
	for (SystemData* system : SystemData::GetSystems())
	{
		if (system->getName() != "retropie")
		{
			if (( target - ( int ) system->getDisplayedGameCount() ) >= 0)
			{
				target -= ( int ) system->getDisplayedGameCount();
			}
			else
			{
				goToGameList(system);
				std::vector<FileData*> list = system->getRootFolder()->getFilesRecursive(GAME, true);
				getGameListView(system)->setCursor(list.at(target));
				return;
			}
		}
	}
}

void ViewController::playViewTransition(const std::string& transition_style)
{
	Eigen::Vector3f target(Eigen::Vector3f::Identity());
	if (mCurrentView)
		target = mCurrentView->getPosition();

	// no need to animate, we're not going anywhere (probably goToNextGamelist() or goToPrevGamelist() when there's only 1 system)
	if (target == -mCamera.translation() && !isAnimationPlaying(0))
		return;

	if (transition_style == "fade")
	{
		// fade
		// stop whatever's currently playing, leaving mFadeOpacity wherever it is
		cancelAnimation(1);

		auto fadeFunc = [ this ] (float t)
		{
			mFadeOpacity = lerp<float>(0, 1, t);
		};

		const static int FADE_DURATION = 240; // fade in/out time
		const static int FADE_WAIT = 320; // time to wait between in/out
		setAnimation(new LambdaAnimation(fadeFunc, FADE_DURATION), 0, [ this, fadeFunc, target ]
		{
			this->mCamera.translation() = -target;
			updateHelpPrompts();
			setAnimation(new LambdaAnimation(fadeFunc, FADE_DURATION), FADE_WAIT, nullptr, true, 1);
		});

		// fast-forward animation if we're partway faded
		if (target == -mCamera.translation())
		{
			// not changing screens, so cancel the first half entirely
			advanceAnimation(0, FADE_DURATION);
			advanceAnimation(0, FADE_WAIT);
			advanceAnimation(0, FADE_DURATION - ( int ) ( mFadeOpacity * FADE_DURATION ));
		}
		else
		{
			advanceAnimation(0, ( int ) ( mFadeOpacity * FADE_DURATION ));
		}
	}
	else if (transition_style == "slide" || transition_style == "simple slide")
	{
		// slide or simple slide
		setAnimation(new MoveCameraAnimation(mCamera, target));
		updateHelpPrompts(); // update help prompts immediately
	}
	else
	{
		// instant
		setAnimation(new LambdaAnimation(
			[ this, target ] (float t)
		{
			this->mCamera.translation() = -target;
		}, 1));
		updateHelpPrompts();
	}
}

void ViewController::onFileChanged(FileData* file, FileChangeType change)
{
	auto it = mGameListViews.find(file->getSystem());
	if (it != mGameListViews.end())
		it->second->onFileChanged(file, change);
}


void ViewController::StopEasterEgg()
{
	Sound::get(k_easterEggSoundPath)->stop();
	if (mEasterEggImage)
	{
		removeChild(mEasterEggImage);
		delete mEasterEggImage;
		mEasterEggImage = nullptr;
	}
}


void ViewController::PlayEasterEgg()
{
	auto random = rand() % 100;
	if ( random < 50)
	{
		return;
	}
	Sound::get(k_easterEggSoundPath)->play();

	mEasterEggImage = new ImageComponent(mWindow, true);
	if (mHasEasterEggImage)
	{
		mEasterEggImage->setImage(k_easterEggImagePath);

	}
	static const Eigen::Vector2f screenSize(Renderer::getScreenWidth(), Renderer::getScreenHeight());
	const float imageWidth = screenSize.x()*0.15f;
	mEasterEggImage->setResize(imageWidth, 0.f);
	mEasterEggImage->setPosition(
		screenSize.x(),
		screenSize.y() - mEasterEggImage->getSize().y());

	mEasterEggImage->setAnimation(new LambdaAnimation([ this ] (float t)
	{
		const float step = smoothStep(0.0, 1.0, t);
		mEasterEggImage->setPosition(
			( screenSize.x() - mEasterEggImage->getSize().x()*step ),
			( mEasterEggImage->getPosition().y() ));
	}, 250), 0, [this] {
		mEasterEggImage->setAnimation(new LambdaAnimation([ this ] (float t)
		{
			const float step = smoothStep(0.0, 1.0, t);
			mEasterEggImage->setPosition(
				( screenSize.x() - mEasterEggImage->getSize().x()*(1.f-step) ),
				( mEasterEggImage->getPosition().y() ));
		}, 250), 300);
	});

	addChild(mEasterEggImage);
}

void ViewController::launch(FileData* game, Eigen::Vector3f center)
{
	PlayEasterEgg();

	if (game->getType() != GAME)
	{
		LOG(LogError) << "tried to launch something that isn't a game";
		return;
	}

	const bool wasBGMusicPlaying = m_context->GetAudioPlayer()->IsPlaying();
	if (m_context->GetAudioPlayer() &&
		Settings::getInstance()->getBool("BackgroundMusicEnabled"))
	{
		m_context->GetAudioPlayer()->Pause();
	}

	auto resumeAudio = [ wasBGMusicPlaying, this ]
	{
		if (wasBGMusicPlaying && m_context->GetAudioPlayer() &&
			Settings::getInstance()->getBool("BackgroundMusicEnabled"))
		{
			m_context->GetAudioPlayer()->Resume();
		}
	};

	// Hide the current view
	if (mCurrentView)
		mCurrentView->onHide();

	Eigen::Affine3f origCamera = mCamera;
	origCamera.translation() = -mCurrentView->getPosition();

	center += mCurrentView->getPosition();
	stopAnimation(1); // make sure the fade in isn't still playing
	mLockInput = true;

	std::string transition_style = Settings::getInstance()->getString("TransitionStyle");
	if (transition_style == "fade")
	{
		// fade out, launch game, fade back in
		auto fadeFunc = [ this ] (float t)
		{
			//t -= 1;
			//mFadeOpacity = lerp<float>(0.0f, 1.0f, t*t*t + 1);
			mFadeOpacity = lerp<float>(0.0f, 1.0f, t);
		};
		setAnimation(new LambdaAnimation(fadeFunc, 800), 0, [ this, game, fadeFunc, resumeAudio ]
		{
			StopEasterEgg();
			game->getSystem()->launchGame(m_context->GetWindow(), game);
			mLockInput = false;
			setAnimation(new LambdaAnimation(fadeFunc, 800), 0, nullptr, true);
			this->onFileChanged(game, FILE_METADATA_CHANGED);
			resumeAudio();
		});
	}
	else if (transition_style == "slide" || transition_style == "simple slide")
	{
		// move camera to zoom in on center + fade out, launch game, come back in
		setAnimation(new LaunchAnimation(mCamera, mFadeOpacity, center, 800), 0,
			[ this, origCamera, center, game, resumeAudio ]
		{
			StopEasterEgg();
			game->getSystem()->launchGame(m_context->GetWindow(), game);
			mCamera = origCamera;
			mLockInput = false;
			setAnimation(new LaunchAnimation(mCamera, mFadeOpacity, center, 600), 0, nullptr, true);
			this->onFileChanged(game, FILE_METADATA_CHANGED);
			resumeAudio();
		});
	}
	else
	{
		setAnimation(new LaunchAnimation(mCamera, mFadeOpacity, center, 10), 0,
			[ this, origCamera, center, game, resumeAudio ]
		{
			StopEasterEgg();
			game->getSystem()->launchGame(m_context->GetWindow(), game);
			mCamera = origCamera;
			mLockInput = false;
			setAnimation(new LaunchAnimation(mCamera, mFadeOpacity, center, 10), 0, nullptr, true);
			this->onFileChanged(game, FILE_METADATA_CHANGED);
			resumeAudio();
		});
	}
}

std::shared_ptr<IGameListView> ViewController::getGameListView(SystemData* system)
{
	//if we already made one, return that one
	auto exists = mGameListViews.find(system);
	if (exists != mGameListViews.end())
		return exists->second;

	//if we didn't, make it, remember it, and return it
	std::shared_ptr<IGameListView> view;

	bool themeHasVideoView = system->getTheme()->hasView("video");

	//decide type
	GameListViewType selectedViewType = AUTOMATIC;

	std::string viewPreference = Settings::getInstance()->getString("GamelistViewStyle");
	if (viewPreference.compare("basic") == 0)
		selectedViewType = BASIC;
	if (viewPreference.compare("detailed") == 0)
		selectedViewType = DETAILED;
	if (viewPreference.compare("video") == 0)
		selectedViewType = VIDEO;

	if (selectedViewType == AUTOMATIC)
	{
		std::vector<FileData*> files = system->getRootFolder()->getFilesRecursive(GAME | FOLDER);
		for (auto it = files.begin(); it != files.end(); it++)
		{
			if (themeHasVideoView && !( *it )->getVideoPath().empty())
			{
				selectedViewType = VIDEO;
				break;
			}
			else if (!( *it )->getThumbnailPath().empty())
			{
				selectedViewType = DETAILED;
				// Don't break out in case any subsequent files have video
			}
		}
	}

	// Create the view
	switch (selectedViewType)
	{
	case VIDEO:
		view = std::shared_ptr<IGameListView>(new VideoGameListView(*m_context, system->getRootFolder()));
		break;
	case DETAILED:
		view = std::shared_ptr<IGameListView>(new DetailedGameListView(mWindow, system->getRootFolder()));
		break;
		// case GRID placeholder for future implementation.
		//		view = std::shared_ptr<IGameListView>(new GridGameListView(mWindow, system->getRootFolder()));
		//		break;
	case BASIC:
	default:
		view = std::shared_ptr<IGameListView>(new BasicGameListView(mWindow, system->getRootFolder()));
		break;
	}

	view->setTheme(system->getTheme());

	std::vector<SystemData*> sysVec = SystemData::GetSystems();
	int id = std::find(sysVec.begin(), sysVec.end(), system) - sysVec.begin();
	view->setPosition(id * ( float ) Renderer::getScreenWidth(), ( float ) Renderer::getScreenHeight() * 2);

	addChild(view.get());

	mGameListViews[ system ] = view;
	return view;
}

std::shared_ptr<SystemView> ViewController::getSystemListView()
{
	//if we already made one, return that one
	if (mSystemListView)
		return mSystemListView;

	mSystemListView = std::shared_ptr<SystemView>(new SystemView(*m_context));
	addChild(mSystemListView.get());
	mSystemListView->setPosition(0, ( float ) Renderer::getScreenHeight());
	return mSystemListView;
}


bool ViewController::input(InputConfig* config, Input input)
{
	if (mLockInput)
		return true;

	// open menu
	if (config->isMappedTo("start", input) && input.value != 0)
	{
		// open menu
		mWindow->pushGui(new GuiMenu(*m_context));
		return true;
	}

	if (mCurrentView)
		return mCurrentView->input(config, input);

	return false;
}

void ViewController::update(int deltaTime)
{
	if (mCurrentView)
	{
		mCurrentView->update(deltaTime);
	}

	updateSelf(deltaTime);
	if (mEasterEggImage)
	{
		mEasterEggImage->advanceAnimation(0, deltaTime);
	}
}

void ViewController::render(const Eigen::Affine3f& parentTrans)
{
	Eigen::Affine3f trans = mCamera * parentTrans;

	// camera position, position + size
	Eigen::Vector3f viewStart = trans.inverse().translation();
	Eigen::Vector3f viewEnd = trans.inverse() * Eigen::Vector3f(( float ) Renderer::getScreenWidth(), ( float ) Renderer::getScreenHeight(), 0);

	// draw systemview
	getSystemListView()->render(trans);

	// draw gamelists
	for (auto it = mGameListViews.begin(); it != mGameListViews.end(); it++)
	{
		// clipping
		Eigen::Vector3f guiStart = it->second->getPosition();
		Eigen::Vector3f guiEnd = it->second->getPosition() + Eigen::Vector3f(it->second->getSize().x(), it->second->getSize().y(), 0);

		if (guiEnd.x() >= viewStart.x() && guiEnd.y() >= viewStart.y() &&
			guiStart.x() <= viewEnd.x() && guiStart.y() <= viewEnd.y())
			it->second->render(trans);
	}

	if (mWindow->peekGui() == this)
		mWindow->renderHelpPromptsEarly();

	// fade out
	if (mFadeOpacity)
	{
		Renderer::setMatrix(parentTrans);
		Renderer::drawRect(0, 0, Renderer::getScreenWidth(), Renderer::getScreenHeight(), 0x00000000 | ( unsigned char ) ( mFadeOpacity * 255 ));
	}
	if (mEasterEggImage)
	{
		mEasterEggImage->render(parentTrans);
	}
}

void ViewController::preload()
{
	for (SystemData* system : SystemData::GetSystems())
	{
		getGameListView(system);
	}
}

void ViewController::reloadGameListView(IGameListView* view, bool reloadTheme)
{
	for (auto it = mGameListViews.begin(); it != mGameListViews.end(); it++)
	{
		if (it->second.get() == view)
		{
			bool isCurrent = ( mCurrentView == it->second );
			SystemData* system = it->first;
			FileData* cursor = view->getCursor();
			mGameListViews.erase(it);

			if (reloadTheme)
				system->loadTheme();

			std::shared_ptr<IGameListView> newView = getGameListView(system);

			// to counter having come from a placeholder
			if (!cursor->isPlaceHolder())
			{
				newView->setCursor(cursor);
			}

			if (isCurrent)
				mCurrentView = newView;

			break;
		}
	}
	// Redisplay the current view
	if (mCurrentView)
		mCurrentView->onShow();

}

void ViewController::reloadAll()
{
	std::map<SystemData*, FileData*> cursorMap;
	for (auto it = mGameListViews.begin(); it != mGameListViews.end(); it++)
	{
		cursorMap[ it->first ] = it->second->getCursor();
	}
	mGameListViews.clear();

	for (auto it = cursorMap.begin(); it != cursorMap.end(); it++)
	{
		it->first->loadTheme();
		getGameListView(it->first)->setCursor(it->second);
	}

	mSystemListView.reset();
	getSystemListView();

	// update mCurrentView since the pointers changed
	if (mState.viewing == GAME_LIST)
	{
		mCurrentView = getGameListView(mState.getSystem());
	}
	else if (mState.viewing == SYSTEM_SELECT)
	{
		SystemData* system = mState.getSystem();
		goToSystemView(SystemData::GetSystems().front());
		mSystemListView->goToSystem(system, false);
		mCurrentView = mSystemListView;
	}
	else
	{
		goToSystemView(SystemData::GetSystems().front());
	}

	updateHelpPrompts();
}

std::vector<HelpPrompt> ViewController::getHelpPrompts()
{
	std::vector<HelpPrompt> prompts;
	if (!mCurrentView)
		return prompts;

	prompts = mCurrentView->getHelpPrompts();
	prompts.push_back(HelpPrompt("start", "menu"));

	return prompts;
}

HelpStyle ViewController::getHelpStyle()
{
	if (!mCurrentView)
		return GuiComponent::getHelpStyle();

	return mCurrentView->getHelpStyle();
}
