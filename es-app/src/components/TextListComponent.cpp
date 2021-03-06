#include "TextListComponent.h"

TextListComponent::TextListComponent(Window* window) :
	BaseT(window),
	mSelectorImage(window),
	mGCImageHorizontalMargin(0.0f),
	m_gameCollectionImage(window),
#ifdef WIN32
	mGameCollectionImageScale(0.7f)
#else
	mGameCollectionImageScale(1.0f)
#endif
{

	//mSelectorImage.setImage(":/arrow.svg");
	m_gameCollectionImage.setImage(":/star_filled.svg");

	mMarqueeOffset = 0;

	mHorizontalMargin = 0;
	mAlignment = ALIGN_CENTER;

	mFont = Font::get(FONT_SIZE_MEDIUM);
	mUppercase = false;
	mLineSpacing = 1.5f;
	
	mSelectorHeight = static_cast<float>(mFont->getSize());
	mSelectorOffsetY = 0;
	mSelectorColor = 0x000000FF;
	mSelectedColor = 0;
	
	mColors[ 0 ] = 0x0000FFFF;
	mColors[ 1 ] = 0x00FF00FF;
	mColors[ 2 ] = 0xFFFF00FF;
	
	mBar.posY = 0.f;
	mBar.targetPosY = mBar.posY;
	mBar.sizeX = 2.0f;
	mBar.posX = -(mBar.sizeX + 4);
}

void TextListComponent::update(int deltaTime)
{
	listUpdate(deltaTime);
	if (!isScrolling() && size() > 0)
	{
		static const int k_maxWaitTime = 1000; //ms
		static const int k_marqueeDeltaShift = 1;

		const Entry& selectedEntry = mEntries.at(( unsigned int ) mCursor);

		float extraLeftMargin = m_gameCollectionImage.getSize().x() * mGameCollectionImageScale;

		const std::string& text = selectedEntry.name;
		const Eigen::Vector2f textSize = mFont->sizeText(text);

		const float textBoxSize = mSize.x() - mHorizontalMargin * 2 - extraLeftMargin;
		const float exceedingTextSize = textSize.x() - textBoxSize;
		const float extraRightMargin = 5;
		if (exceedingTextSize > 0)
		{
			if (mMarqueeWaitTime > k_maxWaitTime)
			{
				if (mMarqueeGoBack)
				{
					if (mMarqueeOffset < 0)
					{
						mMarqueeOffset += k_marqueeDeltaShift;
					}
					else
					{
						mMarqueeWaitTime = 0;
						mMarqueeGoBack = false;
					}
				}
				else
				{
					if (exceedingTextSize + extraRightMargin + mMarqueeOffset > 0)
					{
						mMarqueeOffset -= k_marqueeDeltaShift;
					}
					else
					{
						mMarqueeWaitTime = 0;
						mMarqueeGoBack = true;
					}
				}
			}
			else
			{
				mMarqueeWaitTime += deltaTime;
			}
		}
	}

	mBar.posY += ( mBar.targetPosY - mBar.posY ) * 0.25f;

	GuiComponent::update(deltaTime);
}

void TextListComponent::render(const Eigen::Affine3f& parentTrans)
{
	Eigen::Affine3f trans = parentTrans * getTransform();
	//Eigen::Affine3f trans = Eigen::Affine3f::Identity();

	std::shared_ptr<Font>& font = mFont;

	if (size() == 0)
	{
		return;
	}
	std::pair<int, int> edges = ComputeListEdgeIndexes();
	RenderSelectorImage(trans, edges.first, edges.second);

	RenderBar(edges.second - edges.first);

	PushClipRect(trans);

	const float entrySize = mFont->getSize() * mLineSpacing;
	float y = 0;
	for ( int i = edges.first; i < edges.second; i++ )
	{
		BaseT::Entry& entry = mEntries.at(( unsigned int ) i);

		unsigned int color = GetColor(i , entry.data.colorId);
		BuildTextCache(entry, color);

		const bool isInAnyGC = IsInGameCollection(i);

		OffsetData offsetData = ComputeOffsetData(trans, i, isInAnyGC);

		const Eigen::Vector2f textSize = entry.data.textCache->metrics.size;
		
		Eigen::Vector3f offset = ComputeOffset(y, i, textSize, offsetData);

		RenderText(trans, offset, entry.data.textCache.get());

		if ( isInAnyGC )
		{
			RenderGCImage(entry.data.imageColorId, trans, offsetData.gcHorizPos, y + offsetData.verticalCenterShift);
		}
		y += entrySize;
	}


	Renderer::popClipRect();

	listRenderTitleOverlay(trans);

	GuiComponent::renderChildren(trans);
}

TextListComponent::OffsetData TextListComponent::ComputeOffsetData(Eigen::Affine3f trans, int i, bool isInAnyGC)
{
	const float fontHeight = mFont->getHeight();

	OffsetData offsetData;
	offsetData.horizMargin = mHorizontalMargin;
	offsetData.gcHorizPos = mHorizontalMargin + mGCImageHorizontalMargin;
	offsetData.extraLeftMargin = 0;
	offsetData.verticalCenterShift = 0;
	if (isInAnyGC)
	{
		const Eigen::Vector2f gcImageSize = m_gameCollectionImage.getSize() * mGameCollectionImageScale;
		//hack: text has some horz shift applied..//
		//gcHorizPos += favImageSize.x() * 0.5f; //		

		const float gcHeight = gcImageSize.y();
		offsetData.verticalCenterShift = ( fontHeight - gcHeight ) * 0.5f;
		offsetData.extraLeftMargin = offsetData.gcHorizPos + gcImageSize.x();

		PushClipRect(trans, offsetData.extraLeftMargin);
		offsetData.horizMargin += offsetData.extraLeftMargin;
	}
	return offsetData;
}

Eigen::Vector3f TextListComponent::ComputeOffset(float y, int i, Eigen::Vector2f textSize, OffsetData& offsetData)
{
	Eigen::Vector3f offset(0, y, 0);

	switch (mAlignment)
	{
	case ALIGN_LEFT:
		offset[ 0 ] = offsetData.horizMargin;
		break;
	case ALIGN_CENTER:
	{
		const float exceeding = textSize.x() - ( mSize.x() - offsetData.extraLeftMargin );
		if (exceeding <= 0.f)
		{
			offset[ 0 ] = ( mSize.x() - textSize.x() ) / 2;
			offset[ 0 ] = std::max(offset[ 0 ], 0.f);
			offsetData.gcHorizPos = offset[ 0 ] - offsetData.extraLeftMargin;
		}
		else
		{
			offset[ 0 ] = offsetData.horizMargin;
		}
	} break;
	case ALIGN_RIGHT:
		offset[ 0 ] = ( mSize.x() - textSize.x() );
		offset[ 0 ] -= offsetData.horizMargin;
		offset[ 0 ] = std::max(offset[ 0 ], 0.f);
		break;
	}

	if (mCursor == i)
		offset[ 0 ] += mMarqueeOffset;

	return offset;
}

void TextListComponent::RenderGCImage(uint32_t imageColorId, Eigen::Affine3f gcTrans, float gcHorizPos, float verticalShift)
{
	m_gameCollectionImage.setColorShift(mColors[ imageColorId ]);
	Eigen::Vector3f gcOffset(gcHorizPos, verticalShift, 0);
	gcTrans.translate(gcOffset);
	{
		const float scale = mGameCollectionImageScale;
		gcTrans.scale(Eigen::Vector3f(scale, scale, scale));
	}
	Renderer::popClipRect(); //pop extra margin
	m_gameCollectionImage.render(gcTrans);
}

void TextListComponent::RenderText(Eigen::Affine3f textTrans, Eigen::Vector3f offset, TextCache* textCache)
{
	textTrans.translate(offset);
	Renderer::setMatrix(textTrans);
	mFont->renderTextCache(textCache);
}

void TextListComponent::RenderSelectorImage(Eigen::Affine3f trans, int startEntry, int listCutoff)
{
	if (startEntry < listCutoff)
	{
		const float entrySize = mFont->getSize() * mLineSpacing;

		if (mSelectorImage.hasImage())
		{
			const float fontHeight = mFont->getHeight();
			const Eigen::Vector2f imgSize = mSelectorImage.getSize();
			const float verticalCenterShift = ( fontHeight - imgSize.y() ) * 0.5f;
			mSelectorImage.setPosition(
				-mSelectorImage.getSize().x(),
				( mCursor - startEntry )*entrySize + verticalCenterShift + mSelectorOffsetY, 0.f);
			mSelectorImage.render(trans);

		}
		else
		{
			Renderer::setMatrix(trans);
			Renderer::drawRect(0.f, ( mCursor - startEntry )*entrySize + mSelectorOffsetY, mSize.x(), mSelectorHeight, mSelectorColor);
		}
	}
}

void TextListComponent::RenderBar(const uint32_t visibleCount)
{
	const uint32_t totalCount = mEntries.size();
	if (visibleCount < totalCount)
	{
		const float rowHeight = GetRowHeight();
		const float visibleListHeight = GetFullyVisibleRowCount() * rowHeight;
		mBar.sizeY = static_cast< float >( visibleCount ) / static_cast< float >( totalCount ) *  visibleListHeight;
		mBar.sizeY = std::max(mBar.sizeY, rowHeight);
		Renderer::drawRect(mBar.posX, mBar.posY, mBar.sizeX, mBar.sizeY, mColors[ 0 ]);
	}
}

uint32_t TextListComponent::GetColor(int i, uint32_t defaultColorId) const
{
	if (mCursor == i && mSelectedColor)
		return mSelectedColor;
	else
		return mColors[ defaultColorId ];

}

void TextListComponent::BuildTextCache(BaseT::Entry& entry, uint32_t color)
{
	if (!entry.data.textCache)
		entry.data.textCache = std::unique_ptr<TextCache>(mFont->buildTextCache(mUppercase ? strToUpper(entry.name) : entry.name, 0, 0, 0x000000FF));

	entry.data.textCache->setColor(color);
}

float TextListComponent::GetRowHeight() const
{
	const float rowHeight = mFont->getSize() * mLineSpacing;
	return rowHeight;
}

uint32_t TextListComponent::GetFullyVisibleRowCount() const
{
	float rowHeight = GetRowHeight();
	uint32_t screenCount = static_cast<uint32_t>( mSize.y() / rowHeight );
	return screenCount;
}

std::pair<int, int> TextListComponent::ComputeListEdgeIndexes() const
{
	int startEntry = 0;
	const float entrySize = mFont->getSize() * mLineSpacing;

	//number of entries that can fit on the screen simultaniously
	uint32_t screenCount = GetFullyVisibleRowCount();

	if (size() >= screenCount)
	{
		startEntry = mCursor - screenCount / 2;
		if (startEntry < 0)
			startEntry = 0;
		if (startEntry >= size() - screenCount)
			startEntry = size() - screenCount;
	}


	int listCutoff = startEntry + screenCount;
	if (listCutoff > size())
		listCutoff = size();
	return std::make_pair(startEntry, listCutoff);
}

bool TextListComponent::input(InputConfig* config, Input input)
{
	if ( size() > 0 )
	{
		if ( input.value != 0 )
		{
			if ( config->isMappedTo("down", input) )
			{
				listInput(1);
				return true;
			}

			if ( config->isMappedTo("up", input) )
			{
				listInput(-1);
				return true;
			}
			if ( config->isMappedTo("pagedown", input) )
			{
				listInput(10);
				return true;
			}

			if ( config->isMappedTo("pageup", input) )
			{
				listInput(-10);
				return true;
			}
		}
		else
		{
			if ( config->isMappedTo("down", input) || config->isMappedTo("up", input) ||
				config->isMappedTo("pagedown", input) || config->isMappedTo("pageup", input) )
			{
				stopScrolling();
			}
		}
	}

	return GuiComponent::input(config, input);
}




//list management stuff
void TextListComponent::add(
	const std::string& name, 
	FileData* obj,
	unsigned int color,
	unsigned int imageColor)
{
	assert(color < COLOR_ID_COUNT);

	BaseT::Entry entry;
	entry.name = name;
	entry.object = obj;
	entry.data.colorId = color;
	entry.data.imageColorId = imageColor;

	BaseT::add(entry);
}

void TextListComponent::onCursorChanged(const CursorState& state)
{
	mMarqueeOffset = 0;
	mMarqueeWaitTime = 0;
	if (mCursorChangedCallback)
	{
		mCursorChangedCallback(state);
	}
	if (mEntries.size() > 0)
	{
		//const int extraCutOff = static_cast<int> (mSize.y()) % mEntries.size();
		const float visibleListHeight = GetFullyVisibleRowCount() * GetRowHeight();
		const float scrollRatio = static_cast< float >( mCursor ) / static_cast< float >( mEntries.size());
		mBar.targetPosY = scrollRatio * ( visibleListHeight - mBar.sizeY );
	}
}



void TextListComponent::applyTheme(const std::shared_ptr<ThemeData>& theme, const std::string& view, const std::string& element, unsigned int properties)
{
	GuiComponent::applyTheme(theme, view, element, properties);

	const ThemeData::ThemeElement* elem = theme->getElement(view, element, "textlist");
	if ( !elem )
		return;

	using namespace ThemeFlags;
	if ( properties & COLOR )
	{
		if ( elem->has("selectorColor") )
			setSelectorColor(elem->get<unsigned int>("selectorColor"));
		if ( elem->has("selectedColor") )
			setSelectedColor(elem->get<unsigned int>("selectedColor"));
		if ( elem->has("primaryColor") )
			setColor(0, elem->get<unsigned int>("primaryColor"));
		if ( elem->has("secondaryColor") )
			setColor(1, elem->get<unsigned int>("secondaryColor"));
		if (elem->has("gameCollectionIconColor"))
		{
			mColors[ 2 ] = elem->get<unsigned int>("gameCollectionIconColor");
		}
		
	}
	if (elem && properties & PATH && elem->has("gameCollectionIconPath"))
	{
		m_gameCollectionImage.setImage(elem->get<std::string>("gameCollectionIconPath"));
	}
	if (elem && properties && elem->has("gameCollectionIconScale"))
	{
		mGameCollectionImageScale = elem->get<float>("gameCollectionIconScale");
	}

	setFont(Font::getFromTheme(elem, properties, mFont));

	if ( properties & SOUND && elem->has("scrollSound") )
		setSound(Sound::get(elem->get<std::string>("scrollSound")));

	if ( properties & ALIGNMENT )
	{
		if ( elem->has("alignment") )
		{
			const std::string& str = elem->get<std::string>("alignment");
			if ( str == "left" )
				setAlignment(ALIGN_LEFT);
			else if ( str == "center" )
				setAlignment(ALIGN_CENTER);
			else if ( str == "right" )
				setAlignment(ALIGN_RIGHT);
			else
				LOG(LogError) << "Unknown TextListComponent alignment \"" << str << "\"!";
		
		}
		if ( elem->has("horizontalMargin") )
		{
			mHorizontalMargin = elem->get<float>("horizontalMargin") * ( this->mParent ? this->mParent->getSize().x() : ( float ) Renderer::getScreenWidth() );
		}
		if (elem->has("gameCollectionIconhorizontalMargin"))
		{
			mGCImageHorizontalMargin = elem->get<float>("gameCollectionIconhorizontalMargin")* ( this->mParent ? this->mParent->getSize().x() : ( float ) Renderer::getScreenWidth() );
		}
	}

	if (properties & FORCE_UPPERCASE && elem->has("forceUppercase"))
	{
		setUppercase(elem->get<bool>("forceUppercase"));
	}

	if (properties & LINE_SPACING)
	{
		if (elem->has("lineSpacing"))
		{ 
			setLineSpacing(elem->get<float>("lineSpacing"));
		}
		if (elem->has("selectorHeight"))
		{
			setSelectorHeight(elem->get<float>("selectorHeight") * Renderer::getScreenHeight());
		}
		else
		{
			setSelectorHeight(mFont->getSize() * 1.5f);

		}
		if (elem->has("selectorOffsetY"))
		{
			float scale = this->mParent ? this->mParent->getSize().y() : ( float ) Renderer::getScreenHeight();
			setSelectorOffsetY(elem->get<float>("selectorOffsetY") * scale);
		}
		else
		{
			setSelectorOffsetY(0.0);
		}
	}

	if (elem->has("selectorImagePath"))
	{
		std::string path = elem->get<std::string>("selectorImagePath");
		bool tile = elem->has("selectorImageTile") && elem->get<bool>("selectorImageTile");
		mSelectorImage.setImage(path, tile);
		mSelectorImage.setSize(mSize.x(), mSelectorHeight);
		mSelectorImage.setColorShift(mSelectorColor);
	}
}
