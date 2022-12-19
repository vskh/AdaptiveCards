// Copyright (c) Microsoft Corporation. All rights reserved.
// Licensed under the MIT License.
#include "pch.h"

#include "AdaptiveImageRenderer.h"
#include "AdaptiveImageRenderer.g.cpp"

#include "ActionHelpers.h"
#include "AdaptiveBase64Util.h"
#include "AdaptiveCardGetResourceStreamArgs.h"
#include <robuffer.h>
#include "WholeItemsPanel.h"
#include "ImageBuilder.h"

namespace winrt::AdaptiveCards::Rendering::Uwp::implementation
{
    AdaptiveImageRenderer::AdaptiveImageRenderer(winrt::com_ptr<render_xaml::XamlBuilder> xamlBuilder) :
        m_xamlBuilder(xamlBuilder)
    {
    }

    winrt::UIElement AdaptiveImageRenderer::Render(winrt::IAdaptiveCardElement const& cardElement,
                                                   winrt::AdaptiveRenderContext const& renderContext,
                                                   winrt::AdaptiveRenderArgs const& renderArgs)
    {
        try
        {
            return m_xamlBuilder->BuildImage(cardElement, renderContext, renderArgs);
        }
        catch (winrt::hresult_error const& ex)
        {
            XamlHelpers::ErrForRenderFailedForElement(renderContext, cardElement.ElementTypeString(), ex.message());
            return nullptr;
        }
    }
}

namespace AdaptiveCards::Rendering::Uwp
{
    //////////////////////////////////////////////////////////////////////////////////////////////////////////////////
    //
    // IMPORTANT! Methods below here are actually XamlBuilder methods. They're defined here because they're only used
    // by the AdaptiveImageRenderer. They exist on XamlBuilder because it's the only place we have that can track
    // loading and lifetime for images.
    //
    //////////////////////////////////////////////////////////////////////////////////////////////////////////////////

    winrt::UIElement render_xaml::XamlBuilder::BuildImage(winrt::IAdaptiveCardElement const& adaptiveCardElement,
                                                          winrt::AdaptiveRenderContext const& renderContext,
                                                          winrt::AdaptiveRenderArgs const& renderArgs)
    {
        auto adaptiveImage = adaptiveCardElement.as<winrt::AdaptiveImage>();

        auto hostConfig = renderContext.HostConfig();
        auto url = adaptiveImage.Url();

        auto imageUrl = GetUrlFromString(hostConfig, url);

        if (imageUrl == nullptr)
        {
            renderContext.AddWarning(winrt::WarningStatusCode::AssetLoadFailed, L"Image not found");
            return nullptr;
        }

        auto isImageSvg = IsSvgImage(HStringToUTF8(url));

        if (isImageSvg)
        {
            SvgImageBuilder svgBuilder = SvgImageBuilder{};
            this->m_imageBuilder = &svgBuilder;
        }
        else
        {
            BitmapImageBuilder bitmapBuilder = BitmapImageBuilder{};
            this->m_imageBuilder = &bitmapBuilder;
        }

        uint32_t pixelWidth = adaptiveImage.PixelWidth();
        uint32_t pixelHeight = adaptiveImage.PixelHeight();

        bool hasExplicitMeasurements = (pixelWidth || pixelHeight);
        bool isAspectRatioNeeded = (pixelWidth && pixelHeight);

        // Get the image's size and style
        winrt::ImageSize size = winrt::ImageSize::None;
        if (!hasExplicitMeasurements)
        {
            size = adaptiveImage.Size();
        }

        if (size == winrt::ImageSize::None && !hasExplicitMeasurements)
        {
            size = hostConfig.Image().ImageSize();
        }

        auto imageStyle = adaptiveImage.Style();
        auto resourceResolvers = renderContext.ResourceResolvers();
        auto backgroundColor = adaptiveImage.BackgroundColor();
        auto isVisible = adaptiveCardElement.IsVisible();

        winrt::FrameworkElement frameworkElement{nullptr};

        // Issue #8125
        if (imageStyle == winrt::ImageStyle::Person)
        {
            winrt::Ellipse ellipse{};
            winrt::Ellipse backgroundEllipse{};

            winrt::Stretch imageStretch = (isAspectRatioNeeded) ? winrt::Stretch::Fill : winrt::Stretch::UniformToFill;

            auto parentElement = renderArgs.ParentElement();

            auto ellipseAsShape = ellipse.as<winrt::Shape>();
            auto backgrondEllipseAsShape = backgroundEllipse.as<winrt::Shape>();

            SetImageOnUIElement(imageUrl, ellipse, resourceResolvers, (size == winrt::ImageSize::Auto), parentElement, ellipseAsShape, isVisible, imageStretch);

            if (size == winrt::ImageSize::None || size == winrt::ImageSize::Stretch || size == winrt::ImageSize::Auto || hasExplicitMeasurements)
            {
                ellipse.Stretch(imageStretch);
                backgroundEllipse.Stretch(imageStretch);
            }
            else
            {
                // Set the stretch for the ellipse - this is different to the stretch used for the image brush
                // above. This will force the ellipse to conform to fit within the confines of its parent.
                ellipse.Stretch(winrt::Stretch::UniformToFill);
                backgroundEllipse.Stretch(winrt::Stretch::UniformToFill);
            }

            if (!backgroundColor.empty())
            {
                // Fill the background ellipse with solid color brush
                auto color = GetColorFromString(HStringToUTF8(backgroundColor));
                backgroundEllipse.Fill(winrt::SolidColorBrush{color});

                // Create a grid to contain the background color ellipse and the image ellipse
                winrt::Grid imageGrid{};
                XamlHelpers::AppendXamlElementToPanel(backgroundEllipse, imageGrid);
                XamlHelpers::AppendXamlElementToPanel(ellipse, imageGrid);

                frameworkElement = imageGrid;
            }
            else
            {
                frameworkElement = ellipse;
            }
        }
        else
        {
            winrt::Image xamlImage{};

            if (!backgroundColor.empty())
            {
                // Create a surrounding border with solid color background to contain the image
                winrt::Border border{};
                auto color = GetColorFromString(HStringToUTF8(backgroundColor));
                border.Background(winrt::SolidColorBrush{color});

                border.Child(xamlImage);

                frameworkElement = border;
            }
            else
            {
                frameworkElement = xamlImage;
            }

            if (isAspectRatioNeeded)
            {
                xamlImage.Stretch(winrt::Stretch::Fill);
            }

            auto parentElement = renderArgs.ParentElement();

            SetImageOnUIElement(imageUrl, xamlImage, resourceResolvers, (size == winrt::ImageSize::Auto), parentElement, frameworkElement, isVisible);
        }

        auto sizeOptions = hostConfig.ImageSizes();

        if (hasExplicitMeasurements)
        {
            if (pixelWidth)
            {
                if (imageStyle == winrt::ImageStyle::Person)
                {
                    frameworkElement.Width(pixelWidth);
                }
                else
                {
                    frameworkElement.MaxWidth(pixelWidth);
                }
            }

            if (pixelHeight)
            {
                if (imageStyle == winrt::ImageStyle::Person)
                {
                    frameworkElement.Height(pixelHeight);
                }
                else
                {
                    frameworkElement.MaxHeight(pixelHeight);
                }
            }
        }
        else
        {
            if (size == winrt::ImageSize::Small || size == winrt::ImageSize::Medium || size == winrt::ImageSize::Large)
            {
                uint32_t imageSize;
                switch (size)
                {
                case winrt::ImageSize::Small:
                {
                    imageSize = sizeOptions.Small();
                    break;
                }

                case winrt::ImageSize::Medium:
                {
                    imageSize = sizeOptions.Medium();
                    break;
                }

                case winrt::ImageSize::Large:
                {
                    imageSize = sizeOptions.Large();
                    break;
                }
                default:
                {
                    throw winrt::hresult_error(E_UNEXPECTED);
                }
                }

                frameworkElement.MaxWidth(imageSize);

                // We don't want to set a max height on the person ellipse as ellipses do not understand preserving
                // aspect ratio when constrained on both axes.
                if (imageStyle != winrt::ImageStyle::Person)
                {
                    frameworkElement.MaxHeight(imageSize);
                }
            }
        }

        auto adaptiveHorizontalAlignmentReference = adaptiveImage.HorizontalAlignment();

        // If the image doesn't have horizontal alignment set, check the render context for a parent value
        if (adaptiveHorizontalAlignmentReference == nullptr)
        {
            adaptiveHorizontalAlignmentReference = renderContext.HorizontalContentAlignment();
        }

        auto adaptiveHorizontalAlignment = GetValueFromRef(adaptiveHorizontalAlignmentReference, winrt::HAlignment::Left);

        switch (adaptiveHorizontalAlignment)
        {
        case winrt::HAlignment::Left:
            frameworkElement.HorizontalAlignment(winrt::HorizontalAlignment::Left);
            break;
        case winrt::HAlignment::Right:
            frameworkElement.HorizontalAlignment(winrt::HorizontalAlignment::Right);
            break;
        case winrt::HAlignment::Center:
            frameworkElement.HorizontalAlignment(winrt::HorizontalAlignment::Center);
            break;
        }

        frameworkElement.VerticalAlignment(winrt::VerticalAlignment::Top);
        XamlHelpers::SetStyleFromResourceDictionary(renderContext, L"Adaptive.Image", frameworkElement);

        auto selectAction = adaptiveImage.SelectAction();

        winrt::hstring altText = adaptiveImage.AltText();

        winrt::AutomationProperties::SetName(frameworkElement, altText);

        return ActionHelpers::HandleSelectAction(
            adaptiveCardElement, selectAction, renderContext, frameworkElement, XamlHelpers::SupportsInteractivity(hostConfig), true);
    }

    template<typename T>
    void render_xaml::XamlBuilder::SetImageOnUIElement(winrt::Uri const& imageUrl,
                                                       T const& uiElement,
                                                       winrt::AdaptiveCardResourceResolvers const& resolvers,
                                                       bool isAutoSize,
                                                       winrt::IInspectable const& parentElement,
                                                       winrt::IInspectable const& imageContainer,
                                                       bool isVisible,
                                                       winrt::Stretch stretch)
    {
        bool mustHideElement = true;

        // Get the image url scheme
        winrt::hstring schemeName = imageUrl.SchemeName();

        // Create a Image to hold the image data.  We use BitmapImage & SvgImageSource in order to allow
        // the tracker to subscribe to the ImageLoaded/Failed events
        this->m_imageBuilder->CreateImage();

        // Get the resolver for the image
        // TODO: resolver case doesn't work right now :)
        if (resolvers)
        {
            auto resolver = resolvers.Get(schemeName);
            // If we have a resolver
            if (resolver)
            {
                if (!m_enableXamlImageHandling && (m_listeners.size() != 0))
                {
                    this->m_imageBuilder->AssignBitmapOptions(winrt::BitmapCreateOptions::None);
                    this->m_imageLoadTracker->TrackImage(this->m_imageBuilder->GetImage());
                }

                // Create the arguments to pass to the resolver
                auto args = winrt::make<winrt::implementation::AdaptiveCardGetResourceStreamArgs>(imageUrl);

                // And call the resolver to get the image stream
                auto getResourceStreamOperation = resolver.GetResourceStreamAsync(args);

                getResourceStreamOperation.Completed(
                    [this, weakThis = this->get_weak(), uiElement, stretch, isAutoSize, parentElement, imageContainer, isVisible](
                        winrt::IAsyncOperation<winrt::IRandomAccessStream> const& operation, winrt::AsyncStatus status) -> void
                    {
                        if (status == winrt::AsyncStatus::Completed)
                        {
                            if (auto strongThis = weakThis.get())
                            {
                                auto randomAccessStream = operation.GetResults();
                                if (!randomAccessStream)
                                {
                                    this->m_imageLoadTracker->MarkFailedLoadImage(this->m_imageBuilder->GetImage());
                                    return;
                                }
                                SetImageSource(uiElement, this->m_imageBuilder->GetImage(), stretch);

                                this->m_imageBuilder->SetAsyncSource(randomAccessStream,
                                                                    isAutoSize,
                                                                    [weakThis, uiElement, parentElement, imageContainer, isVisible]() {
                                                                        if (auto strongThis = weakThis.get())
                                                                        {
                                                                            strongThis->SetAutoSize(uiElement,
                                                                                                    parentElement,
                                                                                                    imageContainer,
                                                                                                    isVisible,
                                                                                                    false /* imageFiresOpenEvent */);
                                                                            }
                                                                    });
                            }
                        }
                        else
                        {
                            if (auto strongThis = weakThis.get())
                            {
                                // Question: should we only mark as a failed image if (!m_enableXamlImageHandling && (m_listeners.size() != 0))
                                this->m_imageLoadTracker->MarkFailedLoadImage(this->m_imageBuilder->GetImage());
                            }
                        }
                    });
            }
        }

        if (schemeName == L"data")
        {
            // Decode base 64 string
            winrt::hstring dataPath = imageUrl.Path();
            std::string data = AdaptiveBase64Util::ExtractDataFromUri(HStringToUTF8(dataPath));
            std::vector<char> decodedData = AdaptiveBase64Util::Decode(data);

            winrt::DataWriter dataWriter{winrt::InMemoryRandomAccessStream{}};

            dataWriter.WriteBytes(std::vector<byte>{decodedData.begin(), decodedData.end()});

            this->m_imageBuilder->AssignBitmapOptions(winrt::BitmapCreateOptions::IgnoreImageCache);
            m_imageLoadTracker->TrackImage(this->m_imageBuilder->GetImage());

            auto streamWriteOperation = dataWriter.StoreAsync();

            streamWriteOperation.Completed(
                [weakThis = this->get_weak(), dataWriter, uiElement, isAutoSize, parentElement, imageContainer, isVisible](
                    winrt::IAsyncOperation<uint32_t> const& /*operation*/, winrt::AsyncStatus /*status*/) -> void
                {
                    if (auto strongThis = weakThis.get())
                    {
                        if (const auto stream = dataWriter.DetachStream().try_as<winrt::InMemoryRandomAccessStream>())
                        {
                            stream.Seek(0);
                            strongThis->SetImageSource(uiElement, strongThis->m_imageBuilder->GetImage());
                            // TODO: I think we'll have issues here because stream is a different type
                            strongThis->m_imageBuilder->SetAsyncSource(stream,
                                                                     isAutoSize,
                                                                     [weakThis, uiElement, parentElement, imageContainer, isVisible]()
                                                                     {
                                                                         if (auto strongThis = weakThis.get())
                                                                         {
                                                                             strongThis->SetAutoSize(uiElement,
                                                                                                     parentElement,
                                                                                                     imageContainer,
                                                                                                     isVisible,
                                                                                                     false /* imageFiresOpenEvent */);
                                                                         }
                                                                     });
                        }
                    }
                });
            m_writeAsyncOperations.push_back(streamWriteOperation);
            mustHideElement = false;
            return;
        }

        // TODO: this case threw a strange error but app didn't crash
        // Otherwise, no resolver...
        if ((m_enableXamlImageHandling) || (m_listeners.size() == 0))
        {
            // If we've been explicitly told to let Xaml handle the image loading, or there are
            // no listeners waiting on the image load callbacks, use Xaml to load the images

            this->m_imageBuilder->SetUriSource(imageUrl);
            SetImageSource(uiElement, this->m_imageBuilder->GetImage(), stretch);

            // Issue #8126
            if (isAutoSize)
            {
                SetAutoSize(uiElement, parentElement, imageContainer, isVisible, true /* imageFiresOpenEvent */);
            }
        }
        else
        {
            PopulateImageFromUrlAsync(imageUrl, uiElement);
        }
    }

    // Issue #8127
    template<typename T>
    void render_xaml::XamlBuilder::PopulateImageFromUrlAsync(winrt::Uri const& imageUrl, T const& imageControl)
    {
        winrt::HttpBaseProtocolFilter httpBaseProtocolFilter{};
        httpBaseProtocolFilter.AllowUI(false);

        winrt::HttpClient httpClient{httpBaseProtocolFilter};

        this->m_imageLoadTracker->TrackImage(this->m_imageBuilder->GetImage());
        this->m_imageBuilder->AssignBitmapOptions(winrt::BitmapCreateOptions::None);

        auto getStreamOperation = httpClient.GetInputStreamAsync(imageUrl);
        getStreamOperation.Completed(
            [this, weakThis = this->get_weak(), imageControl](
                winrt::IAsyncOperationWithProgress<winrt::IInputStream, winrt::HttpProgress> const& operation,
                winrt::AsyncStatus status) -> void
            {
                if (status == winrt::AsyncStatus::Completed)
                {
                    if (auto strongThis = weakThis.get())
                    {
                        auto imageStream = operation.GetResults();
                        winrt::InMemoryRandomAccessStream randomAccessStream{};
                        auto copyStreamOperation = winrt::RandomAccessStream::CopyAsync(imageStream, randomAccessStream);

                        m_copyStreamOperations.push_back(copyStreamOperation);

                        copyStreamOperation.Completed(
                            [randomAccessStream](winrt::IAsyncOperationWithProgress<uint64_t, uint64_t> const& /*operation*/,
                                                 winrt::AsyncStatus /*status*/) { randomAccessStream.Seek(0); });
                    }
                }
            });
        m_getStreamOperations.push_back(getStreamOperation);
    }

    template<typename T>
    void render_xaml::XamlBuilder::SetImageSource(T const& destination, winrt::ImageSource const& imageSource, winrt::Stretch /*stretch*/)
    {
        destination.Source(imageSource);
    };

    template<>
    void render_xaml::XamlBuilder::SetImageSource<winrt::Ellipse>(winrt::Ellipse const& destination,
                                                                  winrt::ImageSource const& imageSource,
                                                                  winrt::Stretch stretch)
    {
        winrt::ImageBrush imageBrush{};
        imageBrush.ImageSource(imageSource);

        imageBrush.Stretch(stretch);
        destination.Fill(imageBrush);
    };

    template<>
    void render_xaml::XamlBuilder::SetAutoSize<winrt::Ellipse>(winrt::Ellipse const& ellipse,
                                                               winrt::IInspectable const& parentElement,
                                                               winrt::IInspectable const& imageContainer,
                                                               bool isVisible,
                                                               bool imageFiresOpenEvent)
    {
        // Check if the image source fits in the parent container, if so, set the framework element's size to match the original image.
        if (parentElement && m_enableXamlImageHandling)
        {
            auto ellipseAsShape = imageContainer.as<winrt::Shape>();

            auto ellipseBrush = ellipseAsShape.Fill();

            auto brushAsImageBrush = ellipseBrush.as<winrt::ImageBrush>();

            auto imageSource = brushAsImageBrush.ImageSource();

            // If the image hasn't loaded yet
            if (imageFiresOpenEvent)
            {
                // Collapse the Ellipse while the image loads, so that resizing is not noticeable
                ellipse.Visibility(winrt::Visibility::Collapsed);

                // Handle ImageOpened event so we can check the imageSource's size to determine if it fits in its parent
                // Take a weak reference to the parent to avoid circular references (Parent->Ellipse->ImageBrush->Lambda->(Parent))
                auto weakParent = winrt::make_weak(parentElement);

                brushAsImageBrush.ImageOpened(
                    [ellipse, weakParent, isVisible](winrt::IInspectable const& sender, winrt::RoutedEventArgs /*args*/) -> void
                    {
                        if (isVisible)
                        {
                            auto lambdaBrushAsImageBrush = sender.as<winrt::ImageBrush>();

                            auto lambdaImageSource = lambdaBrushAsImageBrush.ImageSource();

                            auto lambdaParentElement = weakParent.get();
                            if (ellipse && lambdaParentElement)
                            {
                                winrt::FrameworkElement k{nullptr};
                                winrt::BitmapSource as{nullptr};

                                XamlHelpers::SetAutoImageSize(ellipse, lambdaParentElement, lambdaImageSource, isVisible);
                            }
                        }
                    });
            }
            else
            {
                XamlHelpers::SetAutoImageSize(ellipse, parentElement, imageSource, isVisible);
            }
        }
    }

    template<typename T>
    void render_xaml::XamlBuilder::SetAutoSize(T const& destination,
                                               winrt::IInspectable const& parentElement,
                                               winrt::IInspectable const&, /* imageContainer */
                                               bool isVisible,
                                               bool imageFiresOpenEvent)
    {
        if (parentElement && m_enableXamlImageHandling)
        {
            auto xamlImage = destination.as<winrt::Image>();
            auto imageSource = xamlImage.Source();

            // If the image hasn't loaded yet
            if (imageFiresOpenEvent)
            {
                // Collapse the Image control while the image loads, so that resizing is not noticeable
                xamlImage.Visibility(winrt::Visibility::Collapsed);

                // Handle ImageOpened event so we can check the imageSource's size to determine if it fits in its parent
                // Take weak references to the image and parent to avoid circular references between this lambda and
                // its parents (Parent->Image->Lambda->(Parent and Image))
                auto weakParent = winrt::make_weak(parentElement);

                auto weakImage = winrt::make_weak(xamlImage);

                xamlImage.ImageOpened(
                    [weakImage, weakParent, imageSource, isVisible](winrt::IInspectable const& /*sender*/,
                                                                                  winrt::RoutedEventArgs const&
                                                                                  /*args*/) -> void
                    {
                        if (const auto lambdaImageAsFrameworkElement = weakImage.get())
                        {
                            if (const auto lambdaParentElement = weakParent.get())
                            {
                                XamlHelpers::SetAutoImageSize(lambdaImageAsFrameworkElement, lambdaParentElement, imageSource, isVisible);
                            }
                        }
                    });
            }
            else
            {
                XamlHelpers::SetAutoImageSize(xamlImage, parentElement, imageSource, isVisible);
            }
        }
    }

    boolean render_xaml::XamlBuilder::IsSvgImage(std::string url)
    {
        // Question: is this check sufficient?
        auto foundSvg = url.find("svg");
        return !(foundSvg == std::string::npos);
    }
}
