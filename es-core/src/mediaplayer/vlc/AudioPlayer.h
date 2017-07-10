#pragma once
#include <memory>
#include <string>
#include "../IAudioPlayer.h"


struct libvlc_instance_t;

namespace mediaplayer
{
	namespace vlc
	{
		namespace detail
		{
			class audioplayer;
		}

		class AudioPlayer : public IAudioPlayer
		{
		public:
			AudioPlayer();
			AudioPlayer(libvlc_instance_t& vlcInstance);
			~AudioPlayer() override;

			void Play(const std::string& path) override;
			void Pause() override;
			void Resume() override;
			void Stop() override;
			void Next() override;
			void Prev() override;
			void SetOnEventCallback(const OnEventCallback& c) override;
			void StartPlaylist() override;
			void AddToPlaylist(const std::string path) override;
			void AddToPlaylist(const std::vector<std::string>& paths) override;
			void AddToPlaylist(std::vector<std::string>& paths, ShuffleE shuffle) override;
			void ClearPlaylist() override;
			void SetPlaybacktMode(PlaybackModeE mode);
			bool IsPlaying() const;
			std::size_t PlaylistSize() const override;

			static void Shuffle(std::vector<std::string>& list);

		private:
			std::unique_ptr<detail::audioplayer> m_impl;
			std::string m_path;
		};
	}
}
